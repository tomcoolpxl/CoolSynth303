#pragma once

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "Pattern.h"
#include "Sequencer.h"

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

/*
    CoolSynth 303's processor: the 303's circuit (the engine's Tb303Voice, CoolSynth's D295) played by the app's
    sequencer, then a drive and a tempo-synced delay (D296), each switchable.

    The voice runs at twice the host's rate (96 kHz at 48, 88.2 at 44.1; D294 chose 96 kHz) and comes down through
    the engine's half-band decimator. The drive runs at the voice's rate, so its harmonics fold less.

    Threads, as CoolSynth 808's: the project lives on the message thread; every edit is copied for the audio thread
    under a lock the audio thread only tries (a block that finds it taken plays on with what it had). Play, stop,
    the pattern to hear, the transposition and the keys' audition reach it as atomics; where it plays comes back as
    atomics for the panel.
*/
namespace cs303
{
    class Processor final : public juce::AudioProcessor
    {
    public:
        Processor();
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
        /** The pattern to hear: at once while stopped, at the end of the one that plays while playing. */
        void hearPattern(int index, bool variationB) noexcept { patternRequest.store(index * 2 + (variationB ? 1 : 0)); }
        void setTranspose(int semitones) noexcept { transposeRequest.store(semitones); }
        [[nodiscard]] int transposition() const noexcept { return transposeRequest.load(); }
        /** A key held on the panel while stopped: the voice plays it (key 0..36), -1 lets it go. */
        void audition(int key) noexcept { auditionKey.store(key); }
        void setRecording(bool on) noexcept { recording.store(on); }
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
        /** How much of the time the audio thread spent rendering, smoothed (0..1 of the block's own time). */
        [[nodiscard]] float load() const noexcept { return loadNow.load(); }

        /** Renders while stopped from a key, for tests: the voice's own path without the sequencer. */
        static constexpr int maxBlock = 4096;

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
        void renderSpan(float* out, int start, int count) noexcept;
        void renderVoice(float* v, int n, int key, double beatPerVoiceSample, double secondsPerStep, const Panel& panel) noexcept;
        void finishSpan(float* out, int start, int count, const Panel& panel) noexcept;
        void sendClock(juce::MidiBuffer& midi, int numSamples) noexcept;

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

        // audio thread
        std::unique_ptr<AudioCopy> audio;
        Sequencer sequencer;
        std::unique_ptr<coolsynth::synth::v3::Tb303Voice> voice;
        std::unique_ptr<coolsynth::synth::HalfBandDecimator2x> decimator;
        std::unique_ptr<coolsynth::synth::v3::DriveProcessor> drive;
        std::unique_ptr<coolsynth::synth::GlobalDelay> delay;
        std::vector<float> voiceBuffer, driveScratch;
        juce::AudioBuffer<float> stereo;
        double hostRate = 48000.0, voiceRate = 96000.0;
        bool playing = false;
        double beat = 0.0;
        std::int64_t sixteenth = -1;
        double clockBeatSent = 0.0;
        bool clockStartPending = false, clockStopPending = false;
        double dcState = 0.0, dcIn = 0.0;
        double gainNow = 0.0;
        int lastAudition = -1;
        double auditionVolts = 2.0;
        bool auditionSlide = false;
        std::array<float, knobCount> stepKnobs { none, none, none, none, none };
        static constexpr double restAfterSeconds = 4.0; // silent this long while stopped: the circuit rests
        std::int64_t quietVoiceSamples = 0;
        bool resting = false;
        double levelFollow = 0.0;
        double loadFollow = 0.0;

        // audio thread to the panel
        std::atomic<bool> playingNow { false };
        std::atomic<int> patternNow { 0 }, stepNow { -1 }, queuedNow { -1 }, chainNow { 0 }, soundingNow { -1 };
        std::atomic<bool> variationBNow { false };
        std::atomic<float> levelNow { 0.0f }, loadNow { 0.0f };
        std::array<std::atomic<float>, knobCount> liveKnobs {};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Processor)
    };

    /** The voice's volts to the app's full scale: the 303's output, its DC taken off by C38, at about -6 dBFS. */
    inline constexpr double outputScale = 0.5;
}
