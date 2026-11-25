#pragma once

#include <atomic>
#include <cstdint>

#include <juce_audio_basics/juce_audio_basics.h>

#include "Pattern.h"

/*
    The WAV export (CoolSynth's D355), as CoolSynth 808's (D331): the pattern shown or the chain, played from its start
    a number of times and rendered offline by a processor of its own at Best, as the app plays it (the panel, the
    locks and motion, Drive and Delay). The mix, and as an option the dry voice too: the same render with Drive and
    Delay off, straight from the circuit, the same length as the mix.

    The end: cut where the last time through ends; the ring-out after it; or a seamless loop, the ring-out in its
    start: played round as a loop until what rang before has rung out, then the next time through kept, so its start
    holds the end's ring-out as a loop plays it. 24-bit WAV at the app's rate or at 96 kHz, the voice's own.
*/
namespace cs303
{
    enum class ExportEnd { cut = 0, ring = 1, loop = 2 };

    struct ExportJob
    {
        Project project;
        bool chain = false;      // the chain, or the pattern below
        int pattern = 0;         // 0..15
        bool variationB = false;
        int times = 1;
        ExportEnd end = ExportEnd::cut;
        bool dry = false;        // the dry voice too
        double sampleRate = 48000.0;
    };

    struct ExportResult
    {
        juce::AudioBuffer<float> mix, dry; // dry empty unless asked; both the same length
    };

    /** Sixteenth steps in one time through: the pattern's length, or the chain's patterns' lengths by their repeats. */
    [[nodiscard]] int stepsPerPass(const ExportJob& job) noexcept;

    /** The length of the times played through, in samples: where the next time through would begin. */
    [[nodiscard]] std::int64_t loopSamples(const ExportJob& job) noexcept;

    /** Renders the mix and, if asked, the dry voice, each on a core of its own. Any thread. `done` counts up to 1;
        `cancel` stops it, returning false. */
    bool renderExport(const ExportJob& job, ExportResult& result, std::atomic<float>& done, const std::atomic<bool>& cancel);

    /** Writes a 24-bit WAV; false if it could not. */
    bool writeWav24(const juce::File& file, const juce::AudioBuffer<float>& audio, double sampleRate);

    /** The dry voice's file beside the mix's: "Name - dry.wav". */
    [[nodiscard]] juce::File dryFile(const juce::File& mixFile);
}
