// The pattern model: keys and octaves, JSON, and the tools.

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include "MidiExport.h"
#include "Pattern.h"

namespace
{
    using namespace cs303;

    class PatternTests final : public juce::UnitTest
    {
    public:
        PatternTests() : juce::UnitTest("Patterns", "CoolSynth303") {}

        void runTest() override
        {
            beginTest("a_key_on_the_three_octave_piano_is_a_note_and_an_octave");
            {
                Step s;
                for (int k = 0; k <= 36; ++k)
                {
                    s.setKey(k);
                    expectEquals(s.key(), k);
                    expect(s.note >= 0 && s.note <= 12 && s.octave >= -1 && s.octave <= 1);
                }
                s.setKey(0);
                expectEquals(s.octave, -1);
                s.setKey(21); // the middle A
                expectEquals(s.octave, 0);
                expectEquals(s.note, 9);
                s.setKey(36);
                expectEquals(s.octave, 1);
                expectEquals(s.note, 12);
            }

            beginTest("a_project_reads_back_from_its_json_unchanged");
            {
                auto p = defaultProject();
                p.pattern(3, true).step[5].lock[static_cast<size_t>(Knob::decay)] = 0.25f;
                p.pattern(3, true).length = 11;
                p.chainOn = true;
                p.loop = false;
                p.bpm = 133.5;
                p.panel.square = true;
                p.panel.delayDivision = 3;
                p.scale = Scale::phrygian;
                p.root = 5;
                const auto back = projectFromJson(toJson(p));
                expect(back == p, "the project after a round trip");
            }

            beginTest("json_that_is_not_a_project_gives_the_defaults");
            {
                const auto p = projectFromJson("{ \"bpm\": 9999, \"patterns\": 3, \"chain\": [] }");
                expectEquals(p.bpm, 300.0);
                expectEquals(static_cast<int>(p.chain.size()), 1);
                const auto q = projectFromJson("not json");
                expect(q.panel == Panel {});
            }

            beginTest("the_same_seed_generates_the_same_line");
            {
                Pattern a, b, c;
                generate(a, 42, Scale::minor, 0);
                generate(b, 42, Scale::minor, 0);
                generate(c, 43, Scale::minor, 0);
                expect(a == b);
                expect(! (a == c));
                expect(a.step[0].gate == Gate::note, "a line starts on a note");
                const auto& degrees = scaleDegrees(Scale::minor);
                for (const auto& s : a.step)
                    expect(std::find(degrees.begin(), degrees.end(), s.note % 12) != degrees.end(), "every note is in the key");
            }

            beginTest("snap_to_key_moves_every_note_into_the_key");
            {
                Pattern p;
                for (int i = 0; i < Pattern::steps; ++i)
                    p.step[static_cast<size_t>(i)].setKey(10 + i);
                snapToKey(p, Scale::dorian, 2);
                const auto& degrees = scaleDegrees(Scale::dorian);
                for (const auto& s : p.step)
                    expect(std::find(degrees.begin(), degrees.end(), ((s.note % 12) - 2 + 12) % 12) != degrees.end());
            }

            beginTest("shift_moves_the_steps_round_within_the_length");
            {
                Pattern p;
                p.length = 8;
                p.step[0].setKey(30);
                p.step[7].accent = true;
                p.step[12].slide = true;
                shift(p, 1);
                expectEquals(p.step[1].key(), 30);
                expect(p.step[0].accent, "the last step comes round to the first");
                expect(p.step[12].slide, "steps past the length stay");
                shift(p, -1);
                expectEquals(p.step[0].key(), 30);
            }

            beginTest("semitones_keep_notes_within_the_three_octaves");
            {
                Pattern p;
                p.step[0].setKey(36);
                p.step[1].setKey(0);
                p.step[2].setKey(11);
                semitone(p, 1);
                expectEquals(p.step[0].key(), 36);
                expectEquals(p.step[1].key(), 1);
                expectEquals(p.step[2].key(), 12);
                semitone(p, -1);
                semitone(p, -1);
                expectEquals(p.step[1].key(), 0);
            }

            beginTest("a_pattern_as_midi_holds_ties_overlaps_slides_and_marks_accents");
            {
                Pattern p;
                p.length = 4;
                for (auto& s : p.step)
                    s.gate = Gate::rest;
                p.step[0].setKey(12); // the middle C: MIDI 36
                p.step[0].gate = Gate::note;
                p.step[0].accent = true;
                p.step[1].gate = Gate::tie;
                p.step[1].slide = true;
                p.step[2].setKey(21);
                p.step[2].gate = Gate::note;
                const auto file = patternToMidi(p, 120.0);
                const auto* track = file.getTrack(0);
                std::vector<std::tuple<int, int, int, int>> notes; // note, velocity, on, off
                for (int i = 0; i < track->getNumEvents(); ++i)
                {
                    const auto* e = track->getEventPointer(i);
                    if (e->message.isNoteOn())
                        notes.emplace_back(e->message.getNoteNumber(), e->message.getVelocity(), juce::roundToInt(e->message.getTimeStamp()),
                                           e->noteOffObject != nullptr ? juce::roundToInt(e->noteOffObject->message.getTimeStamp()) : -1);
                }
                expectEquals(static_cast<int>(notes.size()), 2);
                expectEquals(std::get<0>(notes[0]), 36);
                expectEquals(std::get<1>(notes[0]), 127);
                expectEquals(std::get<2>(notes[0]), 0);
                expect(std::get<3>(notes[0]) > 2 * 240, "the tie and the slide hold it into the third step");
                expectEquals(std::get<0>(notes[1]), 45);
                expectEquals(std::get<1>(notes[1]), 100);
                expectEquals(std::get<2>(notes[1]), 480);
                expectEquals(std::get<3>(notes[1]), 600, "half its step");
            }

            beginTest("mutate_changes_only_what_it_may");
            {
                Pattern p;
                generate(p, 7, Scale::minor, 0);
                const auto before = p;
                mutate(p, 99, Scale::minor, 0, { false, true, false, false });
                for (int i = 0; i < Pattern::steps; ++i)
                {
                    expectEquals(p.step[static_cast<size_t>(i)].note, before.step[static_cast<size_t>(i)].note);
                    expect(p.step[static_cast<size_t>(i)].slide == before.step[static_cast<size_t>(i)].slide);
                    expect(p.step[static_cast<size_t>(i)].gate == before.step[static_cast<size_t>(i)].gate);
                }
            }
        }
    };

    static PatternTests patternTests;
}
