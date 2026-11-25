#include "AudioExport.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>

#include "Processor.h"

namespace cs303
{
    namespace
    {
        constexpr int block = 512;
        constexpr float quietLevel = 1.0e-5f; // -100 dB
        constexpr double quietSeconds = 0.25; // that long under it, and it has rung out
        constexpr double longestRing = 20.0;  // seconds after the end at most
        constexpr double ringBefore = 10.0;   // a seamless loop plays at least this long before the time kept

        double tempo(const ExportJob& job) noexcept { return juce::jlimit(40.0, 300.0, job.project.bpm); } // as the processor plays it

        /** The first sample at or after a beat, as the processor reaches it. */
        std::int64_t sampleAt(const ExportJob& job, double beats) noexcept
        {
            return static_cast<std::int64_t>(std::ceil(beats * job.sampleRate * 60.0 / tempo(job) - 1.0e-6));
        }

        double beatsPerPass(const ExportJob& job) noexcept { return stepsPerPass(job) / 4.0; }

        /** One render, the mix or the dry voice, by a processor of its own. */
        class OneRender
        {
        public:
            OneRender(const ExportJob& j, bool dryVoice, std::atomic<std::int64_t>& counter, const std::atomic<bool>& stop)
                : job(j), rendered(counter), cancel(stop)
            {
                Project project = job.project;
                project.chainOn = job.chain;
                project.loop = true;
                if (dryVoice)
                    project.panel.driveOn = project.panel.delayOn = false;
                processor->replaceProject(project);
                processor->hearPattern(job.pattern, job.variationB);
                processor->setNonRealtime(true); // at Best whatever the app's setting (D349)
                processor->setPlayConfigDetails(0, 2, job.sampleRate, block);
                processor->prepareToPlay(job.sampleRate, block);
            }

            /** The whole render into `out`; the last sample above the quiet in `lastLoud`. False if cancelled. */
            bool run(juce::AudioBuffer<float>& out, std::int64_t& lastLoud)
            {
                const auto loop = loopSamples(job);
                lastLoud = -1;
                processor->setPlaying(true);
                if (job.end == ExportEnd::loop)
                {
                    // Round as a loop until what rang before has rung out (the 303's sequencer plays from the start
                    // only), then the next time through kept.
                    const double passBeats = beatsPerPass(job) * std::max(1, job.times);
                    const double passSeconds = passBeats * 60.0 / tempo(job);
                    const int rounds = std::max(1, static_cast<int>(std::ceil(ringBefore / passSeconds)));
                    if (! play(sampleAt(job, passBeats * rounds), nullptr))
                        return false;
                    const auto kept = sampleAt(job, passBeats * (rounds + 1)) - sampleAt(job, passBeats * rounds);
                    out.setSize(2, static_cast<int>(kept));
                    return play(kept, &out, 0, &lastLoud);
                }

                const auto room = job.end == ExportEnd::ring ? loop + static_cast<std::int64_t>(longestRing * job.sampleRate) : loop;
                out.setSize(2, static_cast<int>(room));
                out.clear();
                if (! play(loop, &out, 0, &lastLoud))
                    return false;
                if (job.end == ExportEnd::cut)
                    return true;
                // The ring-out: stopped, until it has been quiet a while.
                processor->setPlaying(false);
                const auto hold = static_cast<std::int64_t>(quietSeconds * job.sampleRate);
                std::int64_t at = loop;
                while (at < room && (at - std::max(lastLoud, loop - 1)) < hold)
                {
                    const auto n = std::min<std::int64_t>(block, room - at);
                    if (! play(n, &out, at, &lastLoud))
                        return false;
                    at += n;
                }
                return true;
            }

        private:
            bool play(std::int64_t samples, juce::AudioBuffer<float>* out, std::int64_t at = 0, std::int64_t* lastLoud = nullptr)
            {
                for (std::int64_t done = 0; done < samples;)
                {
                    if (cancel.load(std::memory_order_relaxed))
                        return false;
                    const int n = static_cast<int>(std::min<std::int64_t>(block, samples - done));
                    buffer.setSize(2, n, false, false, true);
                    midi.clear();
                    processor->processBlock(buffer, midi);
                    if (out != nullptr)
                        for (int c = 0; c < 2; ++c)
                        {
                            const float* from = buffer.getReadPointer(c);
                            out->copyFrom(c, static_cast<int>(at + done), from, n);
                            if (lastLoud != nullptr)
                                for (int i = 0; i < n; ++i)
                                    if (std::abs(from[i]) > quietLevel)
                                        *lastLoud = std::max(*lastLoud, at + done + i);
                        }
                    done += n;
                    rendered.fetch_add(n, std::memory_order_relaxed);
                }
                return true;
            }

            const ExportJob& job;
            std::atomic<std::int64_t>& rendered;
            const std::atomic<bool>& cancel;
            std::unique_ptr<Processor> processor = std::make_unique<Processor>(true); // offline: no ports, no beat sync (D361)
            juce::AudioBuffer<float> buffer { 2, block };
            juce::MidiBuffer midi;
        };
    }

    int stepsPerPass(const ExportJob& job) noexcept
    {
        const auto lengthOf = [&](int pattern, bool b) { return std::clamp(job.project.pattern(std::clamp(pattern, 0, 15), b).length, 1, Pattern::steps); };
        if (! job.chain)
            return lengthOf(job.pattern, job.variationB);
        int steps = 0;
        const auto count = std::min<std::size_t>(job.project.chain.size(), 64); // PlayBank::maxChain
        for (std::size_t i = 0; i < count; ++i)
        {
            const auto& e = job.project.chain[i];
            steps += lengthOf(e.pattern, e.variationB) * std::clamp(e.repeats, 1, 16);
        }
        return std::max(1, steps);
    }

    std::int64_t loopSamples(const ExportJob& job) noexcept
    {
        return sampleAt(job, beatsPerPass(job) * std::max(1, job.times));
    }

    bool renderExport(const ExportJob& job, ExportResult& result, std::atomic<float>& done, const std::atomic<bool>& cancel)
    {
        const std::size_t count = job.dry ? 2 : 1;
        std::vector<juce::AudioBuffer<float>> audio(count);
        std::vector<std::int64_t> lastLoud(count, -1);
        std::vector<char> finished(count, 0);

        // The work to do, for the progress: the time kept, plus what plays before it or rings after.
        const auto loop = loopSamples(job);
        const auto extra = job.end == ExportEnd::loop ? static_cast<std::int64_t>(ringBefore * job.sampleRate) + loop
                         : job.end == ExportEnd::ring ? static_cast<std::int64_t>(2.0 * job.sampleRate) : 0;
        const double total = static_cast<double>((loop + extra) * static_cast<std::int64_t>(count));
        std::atomic<std::int64_t> rendered { 0 };

        {
            // Each render on a core of its own, below the audio thread's priority: the app keeps playing.
            juce::ThreadPool pool(juce::ThreadPoolOptions {}
                                      .withThreadName("CoolSynth 303 export")
                                      .withNumberOfThreads(static_cast<int>(count))
                                      .withThreadStackSizeBytes(4 * 1024 * 1024)
                                      .withDesiredThreadPriority(juce::Thread::Priority::low));
            for (std::size_t i = 0; i < count; ++i)
                pool.addJob([&, i]
                {
                    OneRender render(job, i == 1, rendered, cancel);
                    finished[i] = render.run(audio[i], lastLoud[i]) ? 1 : 0;
                });
            while (pool.getNumJobs() > 0)
            {
                done.store(static_cast<float>(std::min(0.99, static_cast<double>(rendered.load()) / std::max(1.0, total))));
                juce::Thread::sleep(20);
            }
        }
        if (cancel.load() || std::find(finished.begin(), finished.end(), 0) != finished.end())
            return false;

        // Rung out: both files as long as the longer ring-out, so they line up.
        if (job.end == ExportEnd::ring)
        {
            std::int64_t length = loop;
            for (const auto last : lastLoud)
                length = std::max(length, last + 1);
            for (auto& a : audio)
                a.setSize(2, static_cast<int>(length), true, true, false);
        }
        result.mix = std::move(audio[0]);
        result.dry = count > 1 ? std::move(audio[1]) : juce::AudioBuffer<float>();
        done.store(1.0f);
        return true;
    }

    bool writeWav24(const juce::File& file, const juce::AudioBuffer<float>& audio, double sampleRate)
    {
        (void) file.deleteFile();
        auto stream = std::unique_ptr<juce::OutputStream>(file.createOutputStream());
        if (stream == nullptr)
            return false;
        juce::WavAudioFormat wav;
        auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions {}
                                                      .withSampleRate(sampleRate)
                                                      .withNumChannels(audio.getNumChannels())
                                                      .withBitsPerSample(24));
        if (writer == nullptr)
            return false;
        return writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples());
    }

    juce::File dryFile(const juce::File& mixFile)
    {
        return mixFile.getSiblingFile(mixFile.getFileNameWithoutExtension() + " - dry.wav");
    }
}
