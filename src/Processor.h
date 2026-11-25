#pragma once

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ClockFollower.h"
#include "Pattern.h"
#include "Sequencer.h"
#include "sync/BeatSync.h"
#include "sync/BlockClock.h"
#include "sync/SyncClock.h"

namespace coolsynth::synth
{
    class GlobalDelay;
    class HalfBandDecimator2x;
}
namespace coolsynth::synth::v3
{
    class Tb303Voice;
    class DriveProcessor;
}
namespace coolsynth::mcp
{
    class Server;
}

/*
    CoolSynth 303's processor: the 303's circuit (the engine's Tb303Voice, CoolSynth's D295) played by the app's
    sequencer, then a drive and a tempo-synced delay (D296), each switchable.

    The voice runs at twice the host's rate (96 kHz at 48, 88.2 at 44.1; D294 chose 96 kHz) and comes down through
    the engine's half-band decimator. The drive runs at the voice's rate, so its harmonics fold less.

    Threads, as CoolSynth 808's: the project lives on the message thread; every edit is copied for the audio thread
    under a lock the audio thread only tries (a block that finds it taken plays on with what it had). Play, stop,
    the pattern to hear, the transposition and the keys' audition reach it as atomics; where it plays comes back as
    atomics for the panel.

    The transport: its own, an incoming MIDI clock, a DAW's, or in the standalone app the beat sync (CoolSynth's
    D361), which leads through a play head of its own as a DAW's does.
*/
namespace cs303
{
    /** The beat sync as last chosen (CoolSynth's D361): on, and Play and Stop shared; both on until switched off. Kept
        in the app's settings. */
    struct SyncChoice
    {
        bool on = coolsynth::sync::BeatSync::kDefaultEnabled;
        bool sharing = coolsynth::sync::BeatSync::kDefaultStartStopSharing;
    };
    [[nodiscard]] SyncChoice syncChoiceFrom(const juce::PropertySet& settings);
    void keepSyncChoice(juce::PropertySet& settings, const SyncChoice& choice);
    /** The SYNC key's word: the other peers' count after it, none while alone ("SYNC", "SYNC 2"). */
    [[nodiscard]] juce::String syncKeyText(int peers);

    class Processor final : public juce::AudioProcessor
    {
    public:
        Processor() : Processor(false) {}
        /** A processor that renders offline (the WAV export, D355): no MCP server, no MIDI output keeper, no beat sync,
            even inside the standalone app. */
        explicit Processor(bool offlineRender);
        ~Processor() override;

        // ---------------------------------------------------------------- juce::AudioProcessor
        void prepareToPlay(double sampleRate, int samplesPerBlock) override;
        void releaseResources() override {}
        bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
        void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
        juce::AudioProcessorEditor* createEditor() override;
        bool hasEditor() const override { return true; }
        const juce::String getName() const override { return "CoolSynth 303"; }
        bool acceptsMidi() const override { return true; }
        bool producesMidi() const override { return true; }
        bool isMidiEffect() const override { return false; }
        double getTailLengthSeconds() const override { return 3.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram(int) override {}
        const juce::String getProgramName(int) override { return {}; }
        void changeProgramName(int, const juce::String&) override {}
        void getStateInformation(juce::MemoryBlock& destData) override;
        void setStateInformation(const void* data, int sizeInBytes) override;

        // ---------------------------------------------------------------- the project (message thread)
        [[nodiscard]] const Project& project() const noexcept { return *model; }
        /** Changes the project and hands it to the audio thread; undoable edits keep a step for Undo. */
        void edit(const std::function<void(Project&)>& change, bool undoable = true);
        /** A drag of a knob or a lane is one undo step: the step is kept at its first change. */
        void beginGesture() { inGesture = true; gestureKept = false; }
        void endGesture() { inGesture = false; }
        void replaceProject(const Project& p);
        [[nodiscard]] bool canUndo() const noexcept { return ! undoStack.empty(); }
        [[nodiscard]] bool canRedo() const noexcept { return ! redoStack.empty(); }
        void undo();
        void redo();
        /** Counts every change, so the panel knows when to draw again. */
        [[nodiscard]] std::uint32_t editCount() const noexcept { return edits; }

        /** A knob turned on the panel: while recording and playing, its move is also written to the pattern that
            plays at the step that plays (the motion lane). */
        void knobMoved(Knob k, float value);

        // ---------------------------------------------------------------- live (message thread)
        void setPlaying(bool shouldPlay) noexcept { playRequest.store(shouldPlay ? 1 : 0); }
        [[nodiscard]] bool isPlaying() const noexcept { return playingNow.load(); }
        /** The transport as it will be once a request not yet taken by the audio thread is: what to decide on right after
            asking (CoolSynth's D354: an audition read isPlaying() just after a stop and kept the transport going). */
        [[nodiscard]] bool willPlay() const noexcept
        {
            const int r = playRequest.load();
            return r >= 0 ? r == 1 : playingNow.load();
        }
        /** The pattern to hear: at once while stopped, at the end of the one that plays while playing. */
        void hearPattern(int index, bool variationB) noexcept { patternRequest.store(index * 2 + (variationB ? 1 : 0)); }
        void setTranspose(int semitones) noexcept { transposeRequest.store(semitones); }
        [[nodiscard]] int transposition() const noexcept { return transposeRequest.load(); }
        /** A key held on the panel while stopped: the voice plays it (key 0..36), -1 lets it go. */
        void audition(int key) noexcept { auditionKey.store(key); }
        void setRecording(bool on) noexcept { recording.store(on); }
        /** The Devil Fish's red Accent button: every note accented while it is held (on a Devil Fish only). */
        void holdAccent(bool held) noexcept { accentHeld.store(held); }
        /** Whether the Devil Fish plays now (the audio thread has taken the switch). */
        [[nodiscard]] bool isDevilFishPlaying() const noexcept { return fishNow.load(); }

        // ---------------------------------------------------------------- Under the Lid (D310)
        /** While the lid is open the audio thread reads every drawn part's current and node's voltage (the engine's
            Tb303Lid) lidRate times a second into a ring of lidFrames frames; the lid reads them back. Each frame ends
            with the gate, the accent, the slide and the note's volts. */
        static constexpr int lidRate = 4000, lidFrames = 8192;
        void setLidOpen(bool open) noexcept { lidOpen.store(open); }
        [[nodiscard]] static int lidSeries() noexcept;
        [[nodiscard]] static int lidStride() noexcept { return lidSeries() + 4; }
        /** Frames written since the app started; frame i is at lidFrame(i) while i > lidWritten() - lidFrames. */
        [[nodiscard]] std::int64_t lidWritten() const noexcept { return lidCount.load(std::memory_order_acquire); }
        [[nodiscard]] const float* lidFrame(std::int64_t i) const noexcept
        {
            return lidRing.data() + static_cast<size_t>(i % lidFrames) * static_cast<size_t>(lidStride());
        }
        [[nodiscard]] bool isRecording() const noexcept { return recording.load(); }

        // ---------------------------------------------------------------- where it plays (for the panel)
        [[nodiscard]] int playingPattern() const noexcept { return patternNow.load(); }
        [[nodiscard]] bool playingVariationB() const noexcept { return variationBNow.load(); }
        [[nodiscard]] int playingStep() const noexcept { return stepNow.load(); }
        [[nodiscard]] int queuedPattern() const noexcept { return queuedNow.load(); }
        [[nodiscard]] int chainPosition() const noexcept { return chainNow.load(); }
        /** The key sounding now (0..36), or -1. */
        [[nodiscard]] int soundingKey() const noexcept { return soundingNow.load(); }
        /** The output's level for the panel's lights, 0..1, smoothed. */
        [[nodiscard]] float outputLevel() const noexcept { return levelNow.load(); }
        /** The panel's knob values as the pattern moves them now (locks and motion), for the knobs to follow. */
        [[nodiscard]] float liveKnob(Knob k) const noexcept { return liveKnobs[static_cast<size_t>(k)].load(); }
        /** Whether an incoming MIDI clock runs the transport now, and the tempo it plays at. */
        [[nodiscard]] bool isFollowingClock() const noexcept { return clockFollowed.load(); }
        /** In a DAW: whether the host's transport runs it now. */
        [[nodiscard]] bool isFollowingHost() const noexcept { return hostFollowed.load(); }
        /** The beat sync leads the beat, with other peers in its session (D361). */
        [[nodiscard]] bool isFollowingSync() const noexcept { return syncFollowed.load(std::memory_order_acquire); }
        /** Play pressed with other peers: waiting for the session's next bar line (D361). */
        [[nodiscard]] bool countingIn() const noexcept { return countingInNow.load(std::memory_order_acquire); }
        [[nodiscard]] double tempoNow() const noexcept { return bpmNow.load(); }
        /** How much of the time the audio thread spent rendering, smoothed (0..1 of the block's own time). */
        [[nodiscard]] float load() const noexcept { return loadNow.load(); }

        // ---------------------------------------------------------------- the accuracy (D349; message thread)
        /** The circuits' accuracy: 0 Best; 1 Light, the voice at the host's rate, not near 96 kHz, and every
            circuit's solve to 1 uV but the pitch's (Tb303Voice::setLighter). A host's offline render always runs at
            Best. The app keeps it in its settings, the same for every song. */
        void setAccuracy(int level);
        [[nodiscard]] int accuracy() const noexcept { return accuracyLevel; }
        /** The level the voice runs at now: the setting, or Best while the host renders offline. */
        [[nodiscard]] int accuracyInUse() const noexcept { return accuracyNow; }
        /** The app's own settings file (the accuracy; in the standalone also the devices, the options' page). */
        [[nodiscard]] static juce::PropertiesFile::Options settingsOptions();

        /** Renders while stopped from a key, for tests: the voice's own path without the sequencer. */
        static constexpr int maxBlock = 4096;

        // ---------------------------------------------------------------- Claude's way in (the MCP server)
        /** The output's left channel kept from now for so many seconds (at most 60), for the record tool: the buffer
            is made here, on the message thread, and the audio thread only fills it. */
        void startCapture(double seconds);
        /** Samples captured so far, and the rate they were taken at; capture() holds them. */
        [[nodiscard]] int captured() const noexcept { return captureCount.load(std::memory_order_acquire); }
        [[nodiscard]] double captureRate() const noexcept { return hostRate; }
        [[nodiscard]] const std::vector<float>& capture() const noexcept { return captureBuffer; }
        /** The loudest output sample since the last call, 0..1, for the play tool. */
        [[nodiscard]] float takeLoudest() noexcept { return loudest.exchange(0.0f); }
        /** Starts the app's MCP server (the standalone does, at its start): 127.0.0.1 only. */
        void startMcpServer();
        [[nodiscard]] juce::String mcpStatus() const { return mcpStatusText; }

        // ---------------------------------------------------------------- the beat sync (CoolSynth's D361)
        /** One tempo, beat grid and play state with the other programs on the network that speak the beat sync: the
            standalone app has one (a plugin follows its DAW, an export renders alone). It feeds the transport through
            a play head of its own, as a DAW's does: Play and Stop and the tempo go through it, a start waits for the
            session's next bar line of 4 beats, and while other peers are in the session an incoming MIDI clock is
            not followed. */
        [[nodiscard]] bool hasBeatSync() const noexcept { return beatSync != nullptr; }
        /** Message thread: the SYNC key, Options and Claude's tools. Kept in the app's settings at once. */
        void setBeatSyncOn(bool on);
        [[nodiscard]] bool beatSyncOn() const noexcept { return syncOn.load(std::memory_order_acquire); }
        void setStartStopSharing(bool on);
        [[nodiscard]] bool startStopSharing() const noexcept { return beatSync != nullptr && beatSync->startStopSharing(); }
        /** The other peers in the session; 0 while off. Message thread. */
        [[nodiscard]] int syncPeers() const noexcept { return beatSync != nullptr && beatSyncOn() ? beatSync->peerCount() : 0; }
        /** Message thread, ten times a second: the output's latency from the audio device, and a tempo another peer
            set becomes the panel's (no undo step). */
        void syncTick();
        /** For the tests: a beat sync of its own, its network on the loopback only, or none (then alone, never on a
            socket); its two switches kept in `settings` if given; the host clock in microseconds the audio thread
            reads; each step's host sample since the start, written into `log` (audio thread, allocating: the tests
            only). */
        void useBeatSyncForTesting(bool network, const juce::File& settings = {});
        void setHostClockForTesting(std::function<std::int64_t()> micros) { hostClockForTesting = std::move(micros); }
        void logStepsForTesting(std::vector<double>* log) noexcept { stepLog = log; }

    private:
        struct AudioCopy
        {
            PlayBank bank;
            Panel panel;
            double bpm = 128.0;
        };

        void push();
        void keepUndo();
        void pullFromUi() noexcept;
        void applyPanel(const Panel& p, const StepOut* step) noexcept;
        void setVolumeNow(double turn) noexcept; // both voices' VOLUME (D362)
        void renderSpan(float* out, int start, int count) noexcept;
        void renderVoice(float* v, int n, int key, double beatPerVoiceSample, double secondsPerStep, const Panel& panel) noexcept;
        void finishSpan(float* out, int start, int count, const Panel& panel) noexcept;
        void sendClock(juce::MidiBuffer& midi, int numSamples) noexcept;
        void renderRange(int from, int to) noexcept;
        /** The transport starts or stops at the block's sample `at` (beat 0 for a start not led from outside). */
        void startOrStop(bool wantPlay, bool byClock, bool byHost, double clockBeat, int at) noexcept;

        const bool offline; // the WAV export's: no ports, no beat sync

        // the MCP server (message thread); stopped first when the processor goes
        std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
        std::unique_ptr<coolsynth::mcp::Server> mcp;
        std::unique_ptr<juce::Timer> midiOutKeeper; // the standalone's MIDI output thread kept running
        juce::String mcpStatusText = "Off";
        std::vector<float> captureBuffer;
        std::atomic<bool> captureArmed { false };
        std::atomic<int> captureCount { 0 };
        std::atomic<float> loudest { 0.0f };

        // message thread
        std::unique_ptr<Project> model;
        std::vector<std::unique_ptr<Project>> undoStack, redoStack;
        bool inGesture = false, gestureKept = false;
        std::uint32_t edits = 0;

        // handover
        juce::SpinLock handover;
        std::unique_ptr<AudioCopy> pending;
        std::atomic<bool> dirty { true };
        std::atomic<int> playRequest { -1 };
        std::atomic<int> patternRequest { -1 };
        std::atomic<int> transposeRequest { 0 };
        std::atomic<int> auditionKey { -1 };
        std::atomic<bool> recording { false };
        std::atomic<bool> accentHeld { false };
        std::atomic<bool> lidOpen { false };
        std::vector<float> lidRing; // lidFrames frames of lidStride() floats, allocated once
        std::atomic<std::int64_t> lidCount { 0 };
        int lidEvery = 24, lidPhase = 0;

        // audio thread
        std::unique_ptr<AudioCopy> audio;
        Sequencer sequencer;
        // the stock TB-303 and the Devil Fish (D309): two builds of the circuit, the one the panel picks plays
        std::unique_ptr<coolsynth::synth::v3::Tb303Voice> voice, fishVoice;
        coolsynth::synth::v3::Tb303Voice* active = nullptr;
        void chooseVoice(bool devilFish) noexcept;
        std::unique_ptr<coolsynth::synth::HalfBandDecimator2x> decimator;
        std::unique_ptr<coolsynth::synth::v3::DriveProcessor> drive;
        std::unique_ptr<coolsynth::synth::GlobalDelay> delay;
        std::vector<float> voiceBuffer, driveScratch;
        juce::AudioBuffer<float> stereo;
        double hostRate = 48000.0, voiceRate = 96000.0;
        int factor = 2; // voice samples a host sample
        int accuracyLevel = 0, accuracyNow = 0; // D349: the setting (message thread), the level prepared
        int preparedBlock = 0;
        bool prepared = false;
        bool playing = false, playingByClock = false, playingByHost = false, endedUnderClock = false;
        ClockFollower clock;
        std::int64_t samplesSoFar = 0;
        double blockBpm = 128.0; // the tempo this block plays at: the project's, or the clock's
        double beat = 0.0;
        std::int64_t sixteenth = -1;
        double clockBeatSent = 0.0;
        int clockStartAt = -1; // the block's sample a MIDI start goes out at, or none
        bool clockStopPending = false;
        std::int64_t spanStart = 0; // the host sample the span being rendered starts at
        double volumeNow = -1.0; // the voices' VOLUME as it stands (slewed toward the panel's), -1 before the first panel
        int lastAudition = -1;
        double auditionVolts = 2.0;
        bool auditionSlide = false;
        std::array<float, knobCount> stepKnobs { none, none, none, none, none };
        static constexpr double restAfterSeconds = 4.0; // silent this long while stopped: the circuit rests
        std::int64_t quietVoiceSamples = 0;
        bool resting = false;
        bool restSettled = false; // settled whole while resting, since the last change (D340)
        double levelFollow = 0.0;
        double loadFollow = 0.0;

        // audio thread to the panel
        std::atomic<bool> playingNow { false }, clockFollowed { false }, hostFollowed { false }, fishNow { false };
        std::atomic<double> bpmNow { 128.0 };
        std::atomic<int> patternNow { 0 }, stepNow { -1 }, queuedNow { -1 }, chainNow { 0 }, soundingNow { -1 };
        std::atomic<bool> variationBNow { false };
        std::atomic<float> levelNow { 0.0f }, loadNow { 0.0f };
        std::array<std::atomic<float>, knobCount> liveKnobs {};

        // The beat sync (D361). Its play head gives the block's beats as a DAW's gives its transport: the tempo the
        // block plays at, the beat at its first sample, and playing once the count-in reaches beat 0 in the block.
        struct SyncPlayHead final : juce::AudioPlayHead
        {
            coolsynth::sync::BlockBeats beats {};
            double sampleRate = 48000.0;
            int samples = 0;
            juce::Optional<PositionInfo> getPosition() const override;
        };
        std::unique_ptr<coolsynth::sync::BeatSync> beatSync; // the standalone's, or a test's
        bool syncNetwork = false;                            // message thread: its network switched with it
        juce::File syncSettingsFile;                         // message thread: the tests' settings, else the app's
        std::atomic<bool> syncOn { false };
        std::atomic<std::int64_t> outputLatencyMicros { 0 };
        coolsynth::sync::BlockClock blockClock; // audio thread
        SyncPlayHead syncHead;                  // audio thread
        bool syncInUse = false;                 // audio thread: this block (and the last) the sync's
        bool syncRunning = false;               // audio thread: the last block had the sync on
        double syncPanelTempo = -1.0;           // audio thread: the panel's tempo as last given to the sync
        std::atomic<double> syncPanelTempoNow { -1.0 }, syncBpmNow { 0.0 };
        std::atomic<int> syncPeersNow { 0 };
        std::atomic<bool> syncFollowed { false }, countingInNow { false };
        std::function<std::int64_t()> hostClockForTesting;
        std::vector<double>* stepLog = nullptr;
        std::unique_ptr<juce::Timer> syncTicker;
        void startBeatSync(const SyncChoice& choice, bool network, coolsynth::sync::BeatSync::Options options);
        void keepSyncChoiceNow();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Processor)
    };

    /** The voice's volts to the app's full scale: the 303's OUTPUT jack (VOLUME and the mixer in the voice, D362). At
        the default Volume (0.78) the jack carries 0.65 of the VCA's output, so 0.73 keeps a note where the app's
        level was when Volume was a gain after the voice (0.5 of the VCA's volts times 2 v^3). */
    inline constexpr double outputScale = 0.73;
}
