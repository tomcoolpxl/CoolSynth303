#include "MidiExport.h"

namespace cs303
{
    juce::MidiFile patternToMidi(const Pattern& pattern, double bpm)
    {
        constexpr int step = midiTicksPerBeat / 4, half = step / 2, overlap = step / 8;
        juce::MidiMessageSequence track;
        track.addEvent(juce::MidiMessage::tempoMetaEvent(juce::roundToInt(60000000.0 / juce::jlimit(40.0, 300.0, bpm))), 0.0);
        track.addEvent(juce::MidiMessage::timeSignatureMetaEvent(4, 4), 0.0);
        const int n = juce::jlimit(1, Pattern::steps, pattern.length);
        for (int i = 0; i < n; ++i)
        {
            const auto& s = pattern.step[static_cast<size_t>(i)];
            if (s.gate != Gate::note)
                continue;
            // the note holds through the ties after it; it ends at half its last step, or, sliding, just into the next
            int last = i;
            while (last + 1 < n && pattern.step[static_cast<size_t>(last + 1)].gate == Gate::tie)
                ++last;
            const bool slides = pattern.step[static_cast<size_t>(last)].slide && last + 1 < n
                                && pattern.step[static_cast<size_t>(last + 1)].gate == Gate::note;
            const int start = i * step;
            const int end = slides ? (last + 1) * step + overlap : last * step + half;
            const int note = juce::jlimit(0, 127, 24 + s.key());
            const auto velocity = static_cast<juce::uint8>(s.accent ? 127 : 100);
            track.addEvent(juce::MidiMessage::noteOn(1, note, velocity), start);
            track.addEvent(juce::MidiMessage::noteOff(1, note), end);
        }
        track.addEvent(juce::MidiMessage::endOfTrack(), n * step);
        track.updateMatchedPairs();
        juce::MidiFile file;
        file.setTicksPerQuarterNote(midiTicksPerBeat);
        file.addTrack(track);
        return file;
    }

    juce::File writePatternMidi(const Pattern& pattern, double bpm, const juce::String& name)
    {
        const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("CoolSynth303 MIDI");
        if (! folder.createDirectory())
            return {};
        auto file = folder.getChildFile(juce::File::createLegalFileName(name.isEmpty() ? juce::String("pattern") : name) + ".mid");
        file.deleteFile();
        juce::FileOutputStream out(file);
        if (! out.openedOk() || ! patternToMidi(pattern, bpm).writeTo(out))
            return {};
        return file;
    }
}
