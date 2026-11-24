#pragma once

#include <array>
#include <cstdint>

#include "Pattern.h"

/*
    The 303's sequencer, as the original's CPU plays a pattern (Julian Schmidt's logic-analyser timing, CoolSynth's
    D275): six clocks a sixteenth step, the gate on for three of them, a slide holding it high into the next step,
    accent and slide latched at the step's start, the gate 2 ms behind the clock. A tie holds the note before; a tie
    after a rest is a rest. A slide glides into the next note: the voice's slide switch is on through that next step.

    The panel's knobs are moved per step by the pattern: a step's lock first, then the motion lane, else the knob.

    Plain data for the audio thread: it never allocates or locks. The app hands it a PlayBank, a copy of the
    project's patterns and chain without their names.
*/
namespace cs303
{
    struct PlayPattern
    {
        std::array<Step, Pattern::steps> step {};
        int length = Pattern::steps;
        std::array<std::array<float, Pattern::steps>, knobCount> motion {};

        PlayPattern() { for (auto& m : motion) m.fill(none); }
        explicit PlayPattern(const Pattern& p) : step(p.step), length(p.length), motion(p.motion) {}
    };

    struct PlayBank
    {
        static constexpr int maxChain = 64;
        std::array<std::array<PlayPattern, 2>, Project::patterns> bank {};
        std::array<ChainEntry, maxChain> chain {};
        int chainLength = 1;
        bool chainOn = false;
        bool loop = true;

        PlayBank() = default;
        explicit PlayBank(const Project& p);
    };

    /** What the voice is given for one step. */
    struct StepOut
    {
        bool sounds = false;       // a note or a tie that holds one
        bool continues = false;    // the gate stays high from the step before (a slide into it, or a tie)
        bool holds = false;        // the gate stays high to the step after (this step slides, or the next ties)
        bool accent = false;
        bool slide = false;        // the voice's slide switch through this step
        double volts = 2.0;        // the note as the ladder's volts (1 V an octave, the A key unshifted 2.75 V)
        std::array<float, knobCount> knob { none, none, none, none, none }; // per-step values, or none: the panel's
    };

    class Sequencer final
    {
    public:
        static constexpr int clocksPerStep = 6;
        static constexpr double gateLagSeconds = 0.002;

        void setBank(const PlayBank& b) noexcept { bank = b; }
        [[nodiscard]] const PlayBank& playBank() const noexcept { return bank; }

        /** The pattern to hear: while stopped at once; while playing at the end of the pattern that plays. */
        void requestPattern(int index, bool variationB) noexcept;
        /** Transposition in semitones (the keys in Transpose, or MIDI), from the next step. */
        void setTranspose(int semitones) noexcept { transpose = semitones; }
        [[nodiscard]] int transposition() const noexcept { return transpose; }

        /** Back to the start: the next step played is the first of the chain or of the pattern to hear. */
        void reset() noexcept;

        /** Moves to the step at this sixteenth (counted from the start of play), latching it. Call with each new
            sixteenth in order; a jump resets and starts afresh from the chain's start. */
        void stepTo(std::int64_t sixteenth) noexcept;

        /** The gate at a place within the step that was latched (0..1), at this tempo. */
        [[nodiscard]] bool gateAt(double fraction, double secondsPerStep) const noexcept;

        [[nodiscard]] const StepOut& current() const noexcept { return now; }
        [[nodiscard]] int playingPattern() const noexcept { return pos.pattern; }
        [[nodiscard]] bool playingVariationB() const noexcept { return pos.variationB; }
        [[nodiscard]] int playingStep() const noexcept { return pos.step; }
        [[nodiscard]] int chainPosition() const noexcept { return pos.chainIndex; }
        [[nodiscard]] int queuedPattern() const noexcept { return queued; }
        [[nodiscard]] bool queuedVariationB() const noexcept { return queuedB; }
        /** With the chain's loop off, whether it has played through (nothing sounds from then on). */
        [[nodiscard]] bool ended() const noexcept { return pos.ended; }
        [[nodiscard]] bool started() const noexcept { return startedAny; }

    private:
        struct Position
        {
            int chainIndex = 0;
            int repeatsLeft = 1;
            int pattern = 0;
            bool variationB = false;
            int step = -1; // -1: before the first step
            bool ended = false;
        };

        [[nodiscard]] Position first() const noexcept;
        /** The place after this one, the queue and the chain followed. consumeQueue: take the queued pattern. */
        [[nodiscard]] Position after(const Position& p, bool& tookQueue) const noexcept;
        [[nodiscard]] const PlayPattern& patternAt(const Position& p) const noexcept;
        [[nodiscard]] StepOut resolve(const Position& p, const StepOut& before, const Position& next) const noexcept;

        PlayBank bank {};
        Position pos {};
        StepOut now {};
        std::int64_t lastSixteenth = -1;
        bool startedAny = false;
        int hearPattern = 0;
        bool hearB = false;
        int queued = -1;
        bool queuedB = false;
        int transpose = 0;
    };

    /** The ladder's volts for a key on the three-octave piano (0 the low C) and a transposition. */
    [[nodiscard]] double noteVolts(int key) noexcept;
}
