#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <juce_core/juce_core.h>

/*
    A 303 pattern as the app writes it (CoolSynth's D296, D302): sixteen steps, each a note in one octave (C to the C
    above), an octave down, as written or up, an accent, a slide into the next step and a gate (a note, a tie that
    holds the step before, or a rest); a length of 1 to 16 steps; knob moves recorded per step (the motion lanes) and
    knob values held on single steps (the locks).

    A bank holds sixteen patterns, each with an A and a B; a chain plays patterns in a row, each a number of times,
    in a loop or once through. A project is the bank, the chain, the panel's knobs, the effects and the tempo, saved
    as JSON.
*/
namespace cs303
{
    enum class Gate : std::uint8_t { note = 0, tie, rest };

    /** The knobs a pattern can move: recorded moves (all five) and per-step locks (the first, third to fifth). */
    enum class Knob : int { cutoff = 0, resonance, envMod, decay, accent, count };
    inline constexpr int knobCount = static_cast<int>(Knob::count);
    [[nodiscard]] const char* knobName(Knob k) noexcept;  // "Cutoff", "Resonance", ...
    [[nodiscard]] const char* knobKey(Knob k) noexcept;   // "cutoff", "resonance", ... (the JSON's)
    [[nodiscard]] bool lockable(Knob k) noexcept;         // resonance is not: it is a pot of two gangs, set by hand

    inline constexpr float none = -1.0f; // a motion value or a lock that is not set

    struct Step
    {
        int note = 0;    // 0..12: C to the C above
        int octave = 0;  // -1, 0, 1
        bool accent = false;
        bool slide = false;
        Gate gate = Gate::note;
        std::array<float, knobCount> lock { none, none, none, none, none }; // 0..1, or none

        /** The key on the three-octave piano: 0 the low C (an octave down) to 36 the top C (up, the C above). */
        [[nodiscard]] int key() const noexcept { return (octave + 1) * 12 + note; }
        void setKey(int key) noexcept;
        [[nodiscard]] bool hasLocks() const noexcept;
        bool operator==(const Step&) const = default;
    };

    struct Pattern
    {
        static constexpr int steps = 16;
        std::array<Step, steps> step {};
        int length = steps; // 1..16
        std::array<std::array<float, steps>, knobCount> motion {}; // per knob, per step: 0..1 or none
        std::string name;

        Pattern();
        [[nodiscard]] bool hasMotion(Knob k) const noexcept;
        bool operator==(const Pattern&) const = default;
    };

    struct ChainEntry
    {
        int pattern = 0;     // 0..15
        bool variationB = false;
        int repeats = 1;     // 1..16
        bool operator==(const ChainEntry&) const = default;
    };

    /** The panel: the 303's knobs and switch, then the drive and the delay after it. */
    struct Panel
    {
        float tuning = 0.5f, cutoff = 0.42f, resonance = 0.72f, envMod = 0.55f, decay = 0.48f, accent = 0.62f;
        float volume = 0.78f;
        bool square = false;
        bool driveOn = true;
        float drive = 0.35f, tone = 0.6f;
        bool delayOn = true;
        int delayDivision = 1; // 0 1/16, 1 1/8, 2 3/16, 3 1/4
        float feedback = 0.35f, mix = 0.22f;
        // the Mods panel: the trimmers as fitted to a real unit (CoolSynth's D275), C11 in microfarads
        float tm3 = 0.35f, tm4 = 0.5687f, tm5 = 0.7389f, c11 = 0.82f;
        bool operator==(const Panel&) const = default;
    };

    /** C11's range on the Mods panel, microfarads: the service notes' 1 uF part within its tolerance and past it. */
    inline constexpr float c11Lowest = 0.47f, c11Highest = 1.5f;

    /** The delay's time in beats for a division index. */
    [[nodiscard]] double delayBeats(int division) noexcept;
    [[nodiscard]] const char* delayDivisionName(int division) noexcept;

    enum class Scale : int { minor = 0, phrygian, dorian, chromatic, count };
    [[nodiscard]] const char* scaleName(Scale s) noexcept;
    [[nodiscard]] const std::vector<int>& scaleDegrees(Scale s) noexcept;

    struct Project
    {
        static constexpr int patterns = 16;
        std::array<std::array<Pattern, 2>, patterns> bank {}; // [pattern][0 A, 1 B]
        std::vector<ChainEntry> chain { ChainEntry {} };
        bool chainOn = false; // off: the edited pattern plays in a loop
        bool loop = true;     // the chain's: off plays it once through, then stops
        Panel panel {};
        double bpm = 128.0;
        int root = 0;         // the key's root, 0 C .. 11 B
        Scale scale = Scale::minor;
        std::string name = "Untitled";

        [[nodiscard]] Pattern& pattern(int index, bool variationB) noexcept { return bank[static_cast<size_t>(index)][variationB ? 1 : 0]; }
        [[nodiscard]] const Pattern& pattern(int index, bool variationB) const noexcept { return bank[static_cast<size_t>(index)][variationB ? 1 : 0]; }
        bool operator==(const Project&) const = default;
    };

    // ---------------------------------------------------------------- JSON
    [[nodiscard]] juce::var toVar(const Pattern& p);
    [[nodiscard]] Pattern patternFromVar(const juce::var& v);
    [[nodiscard]] juce::var toVar(const Project& p);
    [[nodiscard]] Project projectFromVar(const juce::var& v);
    [[nodiscard]] juce::String toJson(const Project& p);
    /** A project from JSON; anything missing or out of range takes its default. */
    [[nodiscard]] Project projectFromJson(const juce::String& json);

    // ---------------------------------------------------------------- the tools (each changes one pattern)
    /** A new line in the key: notes, rests, ties, accents and slides, from a seed (the same seed, the same line). */
    void generate(Pattern& p, std::uint32_t seed, Scale scale, int root);
    struct MutateWhat { bool notes = true, accents = true, slides = true, rests = false; };
    /** Changes four steps at random: only what `what` allows. */
    void mutate(Pattern& p, std::uint32_t seed, Scale scale, int root, MutateWhat what);
    /** Every note outside the key moved to the nearest note in it. */
    void snapToKey(Pattern& p, Scale scale, int root);
    /** The whole pattern a step earlier (-1) or later (+1), within its length. */
    void shift(Pattern& p, int direction);
    /** Every note a semitone down or up, the octave following, kept within the three octaves. */
    void semitone(Pattern& p, int direction);

    /** A pattern for a new project: the line the mockup opens with, from its seed. */
    [[nodiscard]] Project defaultProject();
}
