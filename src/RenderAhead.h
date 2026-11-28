#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <limits>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

/*
    Render ahead (CoolSynth's D372), the standalone app's: a worker thread renders the app's audio some 50 ms before
    the sound card asks for it, as REAPER's anticipative processing renders its plugins ahead. The 303's circuit
    averages well under a core, but single blocks (a note's attack, a knob turned fast) can take longer than the
    block lasts; played as the sound card asks, such a block is a dropout. Rendered ahead, a slow block is covered by
    the fast ones around it. Keys, knobs and the transport answer that much later.

    One timeline of samples: the sound card plays sample k of the output when its callback reaches k. The worker
    renders from sample `ahead` on; the first `ahead` samples are silence. Incoming MIDI is stamped on that timeline
    as the callback receives it and handed to the block rendered next, at negative positions (it came before that
    block); the outgoing MIDI (the clock out) goes out in the callback that plays its sample.

    The audio thread only moves samples and MIDI; the worker does the rendering. A worker behind (it never should be,
    unless the circuit takes more than a core on average) leaves the sound card silence, counted in shortfall().
*/
namespace cs303
{
    class RenderAhead final : private juce::Thread
    {
    public:
        /** One block: the output from sample `start` on, its MIDI (incoming at negative positions, the block's own
            outgoing MIDI written back into it), and the host clock in microseconds at which the callback reaches
            `start` (before the device's own output latency). */
        using Render = std::function<void(juce::AudioBuffer<float>& block, juce::MidiBuffer& midi, std::int64_t start,
                                          std::int64_t callbackMicros)>;

        RenderAhead() : juce::Thread("CoolSynth 303 render ahead") {}
        ~RenderAhead() override { stop(); }

        /** Message thread: starts the worker for `rate` and blocks of `block` samples, `aheadSamples` ahead; `clock`
            reads the host clock in microseconds. */
        void start(Render renderOne, double rate, int block, int aheadSamples, std::function<std::int64_t()> clock)
        {
            stop();
            render = std::move(renderOne);
            hostMicros = std::move(clock);
            sampleRate = rate;
            blockSize = std::max(1, block);
            ahead = std::max(blockSize, aheadSamples);
            const int capacity = ahead + 4 * blockSize + 1;
            audioFifo.setTotalSize(capacity);
            audioFifo.reset();
            ring.setSize(2, capacity, false, true, false);
            work.setSize(2, blockSize, false, true, false);
            midiIn.setTotalSize(midiCapacity);
            midiIn.reset();
            midiOut.setTotalSize(midiCapacity);
            midiOut.reset();
            inEvents.assign(static_cast<size_t>(midiCapacity), Event {});
            outEvents.assign(static_cast<size_t>(midiCapacity), Event {});
            blockMidi.clear();
            blockMidi.ensureSize(16384);
            // the first `ahead` samples are silence
            write(nullptr, ahead);
            produced = ahead;
            consumed = 0;
            missing.store(0);
            const auto now = hostMicros();
            publish(now, 0);
            startRealtimeThread(juce::Thread::RealtimeOptions {}.withPriority(9).withApproximateAudioProcessingTime(blockSize, sampleRate));
        }

        /** Message thread: the worker ends (between two blocks). */
        void stop()
        {
            if (isThreadRunning())
            {
                signalThreadShouldExit();
                wake.signal();
                stopThread(4000);
            }
        }

        [[nodiscard]] bool isRunning() const noexcept { return isThreadRunning(); }
        [[nodiscard]] int aheadSamples() const noexcept { return ahead; }
        /** Samples rendered and not yet played: the lead, less what the worker has fallen behind. */
        [[nodiscard]] int buffered() const noexcept { return audioFifo.getNumReady(); }
        /** Samples the sound card got silence for because the worker was behind. */
        [[nodiscard]] std::int64_t shortfall() const noexcept { return missing.load(std::memory_order_relaxed); }

        /** Audio thread: the incoming MIDI handed on, the output taken from what the worker rendered, the outgoing
            MIDI whose samples play in this callback put in `midi`. */
        void exchange(juce::AudioBuffer<float>& out, juce::MidiBuffer& midi) noexcept
        {
            const int n = out.getNumSamples();
            for (const auto m : midi)
                if (m.numBytes > 0 && m.numBytes <= 3)
                {
                    Event e;
                    e.at = consumed + m.samplePosition;
                    e.size = static_cast<std::uint8_t>(m.numBytes);
                    std::copy_n(m.data, m.numBytes, e.bytes);
                    push(midiIn, inEvents, e);
                }
            midi.clear();
            publish(hostMicros(), consumed);

            const int got = std::min(n, audioFifo.getNumReady());
            {
                const auto scope = audioFifo.read(got);
                for (int ch = 0; ch < out.getNumChannels(); ++ch)
                {
                    const int from = std::min(ch, 1);
                    if (scope.blockSize1 > 0)
                        out.copyFrom(ch, 0, ring, from, scope.startIndex1, scope.blockSize1);
                    if (scope.blockSize2 > 0)
                        out.copyFrom(ch, scope.blockSize1, ring, from, scope.startIndex2, scope.blockSize2);
                }
            }
            if (got < n)
            {
                for (int ch = 0; ch < out.getNumChannels(); ++ch)
                    out.clear(ch, got, n - got);
                missing.fetch_add(n - got, std::memory_order_relaxed);
            }

            // the outgoing MIDI for the samples played now (and any a late worker left behind)
            const std::int64_t end = consumed + got;
            while (midiOut.getNumReady() > 0)
            {
                int i1, n1, i2, n2;
                midiOut.prepareToRead(1, i1, n1, i2, n2);
                const auto& e = outEvents[static_cast<size_t>(i1)];
                if (e.at >= end)
                    break;
                midi.addEvent(e.bytes, e.size, static_cast<int>(std::clamp<std::int64_t>(e.at - consumed, 0, std::max(0, n - 1))));
                midiOut.finishedRead(1);
            }
            consumed = end;
            wake.signal();
        }

        /** For the tests: waits until the worker is `ahead` samples ahead of the sound card. */
        bool waitUntilAhead(int milliseconds)
        {
            const auto until = juce::Time::getMillisecondCounter() + static_cast<juce::uint32>(milliseconds);
            while (audioFifo.getNumReady() < ahead)
            {
                if (juce::Time::getMillisecondCounter() > until)
                    return false;
                juce::Thread::sleep(1);
            }
            return true;
        }

    private:
        struct Event
        {
            std::int64_t at = 0;
            std::uint8_t bytes[3] {};
            std::uint8_t size = 0;
        };
        static constexpr int midiCapacity = 4096;

        void run() override
        {
            const juce::ScopedNoDenormals noDenormals;
            while (! threadShouldExit())
            {
                while (! threadShouldExit() && audioFifo.getNumReady() < ahead)
                    renderOne();
                wake.wait(5.0);
            }
        }

        void renderOne() noexcept
        {
            blockMidi.clear();
            while (midiIn.getNumReady() > 0)
            {
                int i1, n1, i2, n2;
                midiIn.prepareToRead(1, i1, n1, i2, n2);
                const auto& e = inEvents[static_cast<size_t>(i1)];
                const auto at = std::clamp<std::int64_t>(e.at - produced, std::numeric_limits<int>::min() / 2, blockSize - 1);
                blockMidi.addEvent(e.bytes, e.size, static_cast<int>(at));
                midiIn.finishedRead(1);
            }
            // the host clock when the callback reaches this block's first sample, from the last callback's
            std::int64_t micros = 0, at = 0;
            read(micros, at);
            const auto when = micros + static_cast<std::int64_t>(std::llround(static_cast<double>(produced - at) * 1.0e6 / sampleRate));
            work.clear();
            render(work, blockMidi, produced, when);
            write(&work, blockSize);
            for (const auto m : blockMidi)
                if (m.numBytes > 0 && m.numBytes <= 3)
                {
                    Event e;
                    e.at = produced + m.samplePosition;
                    e.size = static_cast<std::uint8_t>(m.numBytes);
                    std::copy_n(m.data, m.numBytes, e.bytes);
                    push(midiOut, outEvents, e);
                }
            produced += blockSize;
        }

        void write(const juce::AudioBuffer<float>* from, int count) noexcept
        {
            const auto scope = audioFifo.write(count);
            for (int ch = 0; ch < 2; ++ch)
            {
                const int src = from != nullptr ? std::min(ch, from->getNumChannels() - 1) : 0;
                if (scope.blockSize1 > 0)
                {
                    if (from != nullptr)
                        ring.copyFrom(ch, scope.startIndex1, *from, src, 0, scope.blockSize1);
                    else
                        ring.clear(ch, scope.startIndex1, scope.blockSize1);
                }
                if (scope.blockSize2 > 0)
                {
                    if (from != nullptr)
                        ring.copyFrom(ch, scope.startIndex2, *from, src, scope.blockSize1, scope.blockSize2);
                    else
                        ring.clear(ch, scope.startIndex2, scope.blockSize2);
                }
            }
        }

        static void push(juce::AbstractFifo& fifo, std::vector<Event>& events, const Event& e) noexcept
        {
            const auto scope = fifo.write(1);
            if (scope.blockSize1 > 0)
                events[static_cast<size_t>(scope.startIndex1)] = e; // a full queue drops the event
        }

        // The last callback's host clock and its sample, read by the worker as one (a sequence lock).
        void publish(std::int64_t micros, std::int64_t at) noexcept
        {
            version.fetch_add(1, std::memory_order_acq_rel);
            callbackMicros.store(micros, std::memory_order_relaxed);
            callbackSample.store(at, std::memory_order_relaxed);
            version.fetch_add(1, std::memory_order_acq_rel);
        }
        void read(std::int64_t& micros, std::int64_t& at) const noexcept
        {
            for (;;)
            {
                const auto v = version.load(std::memory_order_acquire);
                micros = callbackMicros.load(std::memory_order_relaxed);
                at = callbackSample.load(std::memory_order_relaxed);
                if ((v & 1u) == 0 && version.load(std::memory_order_acquire) == v)
                    return;
            }
        }

        Render render;
        std::function<std::int64_t()> hostMicros;
        double sampleRate = 48000.0;
        int blockSize = 512, ahead = 2400;
        juce::AbstractFifo audioFifo { 1 }, midiIn { 1 }, midiOut { 1 };
        juce::AudioBuffer<float> ring, work;
        std::vector<Event> inEvents, outEvents;
        juce::MidiBuffer blockMidi;
        std::int64_t produced = 0; // worker
        std::int64_t consumed = 0; // audio thread
        std::atomic<std::uint32_t> version { 0 };
        std::atomic<std::int64_t> callbackMicros { 0 }, callbackSample { 0 };
        std::atomic<std::int64_t> missing { 0 };
        juce::WaitableEvent wake;
    };
}
