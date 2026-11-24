// The 303's sequencer: the gate's timing, slides, ties, lengths, the queue, the chain, the locks.

#include <memory>

#include <juce_core/juce_core.h>

#include "Sequencer.h"

namespace
{
    using namespace cs303;

    Project oneLine()
    {
        Project p;
        auto& pat = p.pattern(0, false);
        for (int i = 0; i < Pattern::steps; ++i)
        {
            pat.step[static_cast<size_t>(i)].setKey(12 + i % 12);
            pat.step[static_cast<size_t>(i)].gate = Gate::note;
        }
        return p;
    }

    class SequencerTests final : public juce::UnitTest
    {
    public:
        SequencerTests() : juce::UnitTest("Sequencer", "CoolSynth303") {}

        void runTest() override
        {
            const double stepSeconds = 15.0 / 120.0;

            beginTest("a_note_is_on_for_three_of_six_clocks_2_ms_behind_the_clock");
            {
                auto s = std::make_unique<Sequencer>();
                s->setBank(PlayBank(oneLine()));
                s->stepTo(0);
                const double lag = Sequencer::gateLagSeconds / stepSeconds;
                expect(! s->gateAt(0.0, stepSeconds), "not yet at the clock");
                expect(! s->gateAt(lag * 0.5, stepSeconds));
                expect(s->gateAt(lag * 1.01, stepSeconds), "on 2 ms after the clock");
                expect(s->gateAt(0.49, stepSeconds));
                expect(s->gateAt(0.5 + lag * 0.9, stepSeconds), "off 2 ms after the third clock");
                expect(! s->gateAt(0.5 + lag * 1.1, stepSeconds));
                expectWithinAbsoluteError(s->current().volts, 2.0, 1e-9); // the middle C, 2 V
                expect(! s->current().slide);
            }

            beginTest("a_slide_holds_the_gate_into_the_next_note_and_glides");
            {
                auto p = oneLine();
                p.pattern(0, false).step[0].slide = true;
                auto s = std::make_unique<Sequencer>();
                s->setBank(PlayBank(p));
                s->stepTo(0);
                expect(s->current().holds);
                expect(s->gateAt(0.99, stepSeconds), "high to the step's end");
                s->stepTo(1);
                expect(s->current().continues);
                expect(s->current().slide, "the slide switch on through the next note");
                expect(s->gateAt(0.0, stepSeconds), "no gap: the note is not struck again");
                expect(! s->gateAt(0.7, stepSeconds), "the next note ends as usual");
                expectWithinAbsoluteError(s->current().volts, 2.0 + 1.0 / 12.0, 1e-9);
            }

            beginTest("a_tie_holds_the_note_before_and_a_tie_after_a_rest_is_silent");
            {
                auto p = oneLine();
                auto& pat = p.pattern(0, false);
                pat.step[1].gate = Gate::tie;
                pat.step[3].gate = Gate::rest;
                pat.step[4].gate = Gate::tie;
                auto s = std::make_unique<Sequencer>();
                s->setBank(PlayBank(p));
                s->stepTo(0);
                expect(s->current().holds, "a step before a tie holds its gate");
                s->stepTo(1);
                expect(s->current().sounds && s->current().continues);
                expectWithinAbsoluteError(s->current().volts, 2.0, 1e-9);
                s->stepTo(2);
                expect(! s->current().continues, "a note after a tie is struck");
                s->stepTo(3);
                expect(! s->current().sounds);
                s->stepTo(4);
                expect(! s->current().sounds);
                expect(! s->gateAt(0.3, stepSeconds));
            }

            beginTest("a_short_pattern_loops_at_its_length");
            {
                auto p = oneLine();
                p.pattern(0, false).length = 5;
                auto s = std::make_unique<Sequencer>();
                s->setBank(PlayBank(p));
                for (int i = 0; i < 12; ++i)
                {
                    s->stepTo(i);
                    expectEquals(s->playingStep(), i % 5);
                }
            }

            beginTest("a_pattern_asked_for_while_playing_comes_in_at_the_end_of_the_one_that_plays");
            {
                auto p = oneLine();
                p.pattern(2, false) = p.pattern(0, false);
                auto s = std::make_unique<Sequencer>();
                s->setBank(PlayBank(p));
                s->stepTo(0);
                s->requestPattern(2, false);
                for (int i = 1; i < 16; ++i)
                {
                    s->stepTo(i);
                    expectEquals(s->playingPattern(), 0);
                }
                expectEquals(s->queuedPattern(), 2);
                s->stepTo(16);
                expectEquals(s->playingPattern(), 2);
                expectEquals(s->playingStep(), 0);
                expectEquals(s->queuedPattern(), -1);
            }

            beginTest("a_chain_plays_its_patterns_their_times_and_once_through_ends");
            {
                auto p = oneLine();
                p.pattern(1, false) = p.pattern(0, false);
                p.pattern(0, false).length = 4;
                p.pattern(1, false).length = 2;
                p.chain = { { 0, false, 2 }, { 1, false, 1 } };
                p.chainOn = true;
                p.loop = false;
                auto s = std::make_unique<Sequencer>();
                s->setBank(PlayBank(p));
                std::vector<int> heard;
                for (int i = 0; i < 12; ++i)
                {
                    s->stepTo(i);
                    heard.push_back(s->ended() ? -1 : s->playingPattern());
                }
                const std::vector<int> expected { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, -1, -1 };
                expect(heard == expected, "two times four steps of 01, two of 02, then the end");
            }

            beginTest("a_lock_wins_over_the_motion_lane_and_both_over_the_knob");
            {
                auto p = oneLine();
                auto& pat = p.pattern(0, false);
                pat.motion[static_cast<size_t>(Knob::cutoff)].fill(0.3f);
                pat.step[2].lock[static_cast<size_t>(Knob::cutoff)] = 0.9f;
                auto s = std::make_unique<Sequencer>();
                s->setBank(PlayBank(p));
                s->stepTo(0);
                expectWithinAbsoluteError(s->current().knob[static_cast<size_t>(Knob::cutoff)], 0.3f, 1e-6f);
                expect(s->current().knob[static_cast<size_t>(Knob::decay)] < 0.0f, "the decay is the knob's");
                s->stepTo(1);
                s->stepTo(2);
                expectWithinAbsoluteError(s->current().knob[static_cast<size_t>(Knob::cutoff)], 0.9f, 1e-6f);
            }

            beginTest("transposition_moves_the_notes_from_the_next_step");
            {
                auto s = std::make_unique<Sequencer>();
                s->setBank(PlayBank(oneLine()));
                s->stepTo(0);
                s->setTranspose(7);
                s->stepTo(1);
                expectWithinAbsoluteError(s->current().volts, 2.0 + 8.0 / 12.0, 1e-9);
            }
        }
    };

    static SequencerTests sequencerTests;
}
