#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "Pattern.h"

/*
    A pattern as a MIDI file, for dragging into a DAW (CoolSynth's D296 asked for it, as Roland Cloud's and Phoscyon's
    do): its length in sixteenths at 960 ticks a beat, on channel 1. A note lasts half its step, as the 303's gate;
    tied steps lengthen it; a slide overlaps the next note by a little, the way a DAW's 303s read a slide. An accent
    is velocity 127, any other note 100. The middle C (the piano's key 12) is MIDI note 36, as this app's MIDI input
    plays it.
*/
namespace cs303
{
    inline constexpr int midiTicksPerBeat = 960;
    [[nodiscard]] juce::MidiFile patternToMidi(const Pattern& pattern, double bpm);
    /** Writes it to a temporary file named after the pattern; returns the file, or an invalid one. */
    juce::File writePatternMidi(const Pattern& pattern, double bpm, const juce::String& name);
}
