// The processor: the circuit played by the sequencer, the effects, MIDI, the state; how long a block takes.

#include <cmath>
#include <memory>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "Processor.h"

namespace
{
    using namespace cs303;

    struct Rendered
    {
        std::vector<float> left;
        int clocks = 0, starts = 0, stops = 0;
        double seconds = 0.0; // the time the blocks took
    };

    std::unique_ptr<Processor> prepared(double rate = 48000.0, int block = 480)
    {
        auto p = std::make_unique<Processor>();
        p->setPlayConfigDetails(0, 2, rate, block);
        p->prepareToPlay(rate, block);
        return p;
    }

    Rendered render(Processor& p, double seconds, double rate = 48000.0, int block = 480, juce::MidiBuffer* firstMidi = nullptr)
    {
        Rendered r;
        juce::AudioBuffer<float> buffer(2, block);
        const int blocks = static_cast<int>(seconds * rate / block);
        for (int b = 0; b < blocks; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0 && firstMidi != nullptr)
                midi = *firstMidi;
            buffer.clear();
            const auto t0 = juce::Time::getHighResolutionTicks();
            p.processBlock(buffer, midi);
            r.seconds += juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - t0);
            for (const auto m : midi)
            {
                const auto msg = m.getMessage();
                r.clocks += msg.isMidiClock() ? 1 : 0;
                r.starts += msg.isMidiStart() ? 1 : 0;
                r.stops += msg.isMidiStop() ? 1 : 0;
            }
            for (int i = 0; i < block; ++i)
                r.left.push_back(buffer.getSample(0, i));
        }
        return r;
    }

    double rms(const std::vector<float>& x, size_t from, size_t to)
    {
        double s = 0.0;
        for (size_t i = from; i < to && i < x.size(); ++i)
            s += static_cast<double>(x[i]) * x[i];
        return std::sqrt(s / std::max<size_t>(1, to - from));
    }

    float peak(const std::vector<float>& x)
    {
        float m = 0.0f;
        for (float v : x)
            m = std::max(m, std::abs(v));
        return m;
    }

    bool allFinite(const std::vector<float>& x)
    {
        for (float v : x)
            if (! std::isfinite(v))
                return false;
        return true;
    }

    class ProcessorTests final : public juce::UnitTest
    {
    public:
        ProcessorTests() : juce::UnitTest("Processor", "CoolSynth303") {}

        void runTest() override
        {
            beginTest("stopped_and_untouched_it_is_silent");
            {
                auto p = prepared();
                const auto r = render(*p, 1.0);
                expect(allFinite(r.left));
                const double level = rms(r.left, 24000, r.left.size());
                expect(level < 1.0e-3, "rms " + juce::String(level, 6));
            }

            beginTest("playing_the_first_line_sounds_within_full_scale_and_keeps_up");
            {
                auto p = prepared();
                p->setPlaying(true);
                const auto r = render(*p, 4.0);
                expect(allFinite(r.left));
                const double level = rms(r.left, 0, r.left.size());
                const float top = peak(r.left);
                logMessage("the first line at 128 BPM: rms " + juce::String(juce::Decibels::gainToDecibels(level), 1) + " dBFS, peak "
                           + juce::String(juce::Decibels::gainToDecibels(static_cast<double>(top)), 1) + " dBFS; rendering took "
                           + juce::String(100.0 * r.seconds / 4.0, 1) + " % of the time it plays");
                expect(level > 0.01, "it sounds");
                expect(top < 1.0f, "it stays below full scale");
                // on this machine; a shared CI runner's cores are slower and busier, so there only the figure is logged
                if (juce::SystemStats::getEnvironmentVariable("CI", {}).isEmpty())
                    expect(r.seconds < 4.0, "it renders faster than it plays");
                expect(p->isPlaying());
                expect(p->playingStep() >= 0);
            }

            beginTest("it_renders_the_same_twice");
            {
                auto a = prepared();
                auto b = prepared();
                a->setPlaying(true);
                b->setPlaying(true);
                const auto ra = render(*a, 1.0);
                const auto rb = render(*b, 1.0);
                expect(ra.left == rb.left);
            }

            beginTest("it_plays_at_44_1_khz_too");
            {
                auto p = prepared(44100.0, 441);
                p->setPlaying(true);
                const auto r = render(*p, 2.0, 44100.0, 441);
                expect(allFinite(r.left));
                expect(rms(r.left, 0, r.left.size()) > 0.01);
            }

            beginTest("at_96_khz_the_voice_runs_at_the_device_rate_and_keeps_up");
            {
                auto p = prepared(96000.0, 960);
                p->setPlaying(true);
                const auto r = render(*p, 2.0, 96000.0, 960);
                expect(allFinite(r.left));
                expect(rms(r.left, 0, r.left.size()) > 0.01);
                logMessage("at 96 kHz: rendering took " + juce::String(100.0 * r.seconds / 2.0, 1) + " % of the time it plays");
                if (juce::SystemStats::getEnvironmentVariable("CI", {}).isEmpty())
                    expect(r.seconds < 2.0, "it renders faster than it plays");
            }

            beginTest("its_transport_sends_midi_clock_24_a_beat");
            {
                auto p = prepared();
                p->edit([](Project& pr) { pr.bpm = 120.0; }, false);
                p->setPlaying(true);
                const auto r = render(*p, 2.0);
                expectEquals(r.starts, 1);
                expect(std::abs(r.clocks - 96) <= 1, "four beats in two seconds: " + juce::String(r.clocks));
                p->setPlaying(false);
                const auto r2 = render(*p, 0.1);
                expectEquals(r2.stops, 1);
            }

            beginTest("at_rest_the_circuit_is_not_solved_and_wakes_for_the_next_note");
            {
                auto p = prepared();
                const auto quiet = render(*p, 6.0);
                logMessage("stopped, after 5 s: peak " + juce::String(juce::Decibels::gainToDecibels(static_cast<double>(peak(std::vector<float>(quiet.left.begin() + 240000, quiet.left.end()))), -200.0), 1) + " dBFS");
                const auto idle = render(*p, 2.0);
                logMessage("stopped and silent: " + juce::String(100.0 * idle.seconds / 2.0, 2) + " % of the time it plays");
                expect(idle.seconds < 0.1, "resting costs next to nothing");
                expect(peak(idle.left) < 1.0e-4f, "silent");
                p->setPlaying(true);
                const auto r = render(*p, 1.0);
                expect(allFinite(r.left));
                expect(rms(r.left, 0, r.left.size()) > 0.01, "it wakes and plays");
                juce::ignoreUnused(quiet);
            }

            beginTest("an_incoming_clock_starts_it_sets_its_tempo_and_stops_it");
            {
                auto p = prepared();
                juce::AudioBuffer<float> buffer(2, 480);
                const double pulseSamples = 48000.0 * 60.0 / 100.0 / 24.0; // 100 BPM
                double nextPulse = 0.0;
                std::int64_t now = 0;
                bool sentStart = false;
                int clocksOut = 0;
                const auto run = [&](double seconds, bool pulses, bool stopAtEnd) {
                    for (int b = 0; b < static_cast<int>(seconds * 100.0); ++b)
                    {
                        juce::MidiBuffer midi;
                        if (pulses)
                        {
                            if (! sentStart)
                            {
                                midi.addEvent(juce::MidiMessage::midiStart(), 0);
                                sentStart = true;
                            }
                            while (nextPulse < static_cast<double>(now + 480))
                            {
                                midi.addEvent(juce::MidiMessage::midiClock(), static_cast<int>(nextPulse - static_cast<double>(now)));
                                nextPulse += pulseSamples;
                            }
                        }
                        if (stopAtEnd && b == static_cast<int>(seconds * 100.0) - 1)
                            midi.addEvent(juce::MidiMessage::midiStop(), 0);
                        p->processBlock(buffer, midi);
                        for (const auto m : midi)
                            clocksOut += m.getMessage().isMidiClock() ? 1 : 0;
                        now += 480;
                    }
                };
                run(2.0, true, false);
                expect(p->isFollowingClock());
                expect(p->isPlaying(), "the clock's start plays it");
                expectWithinAbsoluteError(p->tempoNow(), 100.0, 1.0);
                expectEquals(clocksOut, 0, "it sends no clock of its own while it follows one");
                run(0.2, true, true);
                run(0.1, true, false);
                expect(! p->isPlaying(), "the clock's stop stops it");
            }

            beginTest("a_trimmer_turned_changes_the_voice_and_stock_sets_it_back");
            {
                const auto hold = [](Processor& p) {
                    render(p, 0.2);
                    p.audition(21); // the middle A, 110 Hz with the trimmers at stock
                    return render(p, 0.5).left;
                };
                auto stock = prepared();
                auto turned = prepared();
                turned->edit([](Project& pr) { pr.panel.tm4 = 0.3f; });
                const auto a = hold(*stock);
                const auto b = hold(*turned);
                double difference = 0.0;
                for (size_t i = 0; i < a.size(); ++i)
                    difference += std::abs(static_cast<double>(a[i]) - b[i]);
                expect(difference / static_cast<double>(a.size()) > 1.0e-3, "TM4 retunes the oscillator");
                turned->undo();
                expect(turned->project().panel == Panel {});
            }

            beginTest("the_devil_fish_switch_plays_the_modded_voice_and_off_brings_the_stock_one_back");
            {
                const auto play = [](Processor& p) {
                    p.setPlaying(true);
                    return render(p, 2.0).left;
                };
                auto stock = prepared();
                render(*stock, 0.2);
                const auto a = play(*stock);

                auto fish = prepared();
                fish->edit([](Project& pr) { pr.panel.devilFish = true; });
                render(*fish, 0.2);
                expect(fish->isDevilFishPlaying(), "the switch reaches the audio thread");
                const auto b = play(*fish);
                expect(allFinite(b));
                double difference = 0.0;
                for (size_t i = 0; i < a.size(); ++i)
                    difference += std::abs(static_cast<double>(a[i]) - b[i]);
                expect(difference / static_cast<double>(a.size()) > 1.0e-3, "the Devil Fish sounds different");
                const double levelA = 20.0 * std::log10(rms(a, 0, a.size())), levelB = 20.0 * std::log10(rms(b, 0, b.size()));
                logMessage("line 1, stock " + juce::String(levelA, 1) + " dBFS, Devil Fish at its stock-like settings "
                           + juce::String(levelB, 1) + " dBFS");
                expectLessThan(std::abs(levelA - levelB), 6.0, "at its stock-like settings near the stock's level");

                auto back = prepared();
                back->edit([](Project& pr) { pr.panel.devilFish = true; });
                render(*back, 0.1);
                back->edit([](Project& pr) { pr.panel.devilFish = false; });
                render(*back, 0.1);
                expect(! back->isDevilFishPlaying(), "off again");
                const auto c = play(*back);
                const double levelC = 20.0 * std::log10(rms(c, 0, c.size()));
                // not to the bit: the switch settles the stock circuit at rest where the other ran 0.2 s idle, and its
                // slow capacitors (C13, C42) start the line a hair apart
                expectLessThan(std::abs(levelA - levelC), 0.25, "off, the stock voice plays as before");
            }

            beginTest("the_red_accent_button_accents_every_note_on_a_devil_fish");
            {
                const auto hold = [](bool accent) {
                    auto p = prepared();
                    p->edit([](Project& pr) { pr.panel.devilFish = true; }, false);
                    render(*p, 0.2);
                    p->holdAccent(accent);
                    p->audition(12);
                    return render(*p, 0.4).left;
                };
                const auto plain = hold(false), accented = hold(true);
                const double a = 20.0 * std::log10(rms(plain, 0, plain.size())), b = 20.0 * std::log10(rms(accented, 0, accented.size()));
                logMessage("a held key, plain " + juce::String(a, 1) + " dBFS, with Accent held " + juce::String(b, 1) + " dBFS");
                expectGreaterThan(b, a + 1.0, "louder accented");
            }

            beginTest("in_a_daw_the_host_transport_runs_it_and_a_loop_back_starts_the_pattern_again");
            {
                struct Head final : juce::AudioPlayHead
                {
                    bool isPlaying = false;
                    double ppq = 0.0, bpm = 90.0;
                    juce::Optional<PositionInfo> getPosition() const override
                    {
                        PositionInfo info;
                        info.setIsPlaying(isPlaying);
                        info.setPpqPosition(ppq);
                        info.setBpm(bpm);
                        return info;
                    }
                };
                auto p = prepared();
                Head head;
                p->setPlayHead(&head);
                juce::AudioBuffer<float> buffer(2, 480);
                const auto run = [&](double seconds) {
                    for (int b = 0; b < static_cast<int>(seconds * 100.0); ++b)
                    {
                        juce::MidiBuffer midi;
                        p->processBlock(buffer, midi);
                        if (head.isPlaying)
                            head.ppq += 480.0 / 48000.0 * head.bpm / 60.0;
                    }
                };
                run(0.2);
                expect(! p->isPlaying(), "the host stopped, it waits");
                head.isPlaying = true;
                head.ppq = 8.0;
                run(1.0); // 1.5 beats at 90 BPM: six steps
                expect(p->isPlaying() && p->isFollowingHost());
                expectWithinAbsoluteError(p->tempoNow(), 90.0, 1e-6);
                expectEquals(p->playingStep(), 5);
                head.ppq = 8.0; // the host's loop comes round
                run(0.05);
                expectEquals(p->playingStep(), 0);
                head.isPlaying = false;
                run(0.1);
                expect(! p->isPlaying(), "the host's stop stops it");
                p->setPlayHead(nullptr);
            }

            beginTest("a_held_key_sounds_while_stopped");
            {
                auto p = prepared();
                render(*p, 0.5);
                p->audition(12);
                const auto r = render(*p, 0.5);
                expect(rms(r.left, 4800, r.left.size()) > 0.01);
                expectEquals(p->soundingKey(), 12);
                p->audition(-1);
            }

            beginTest("a_midi_note_while_stopped_plays_and_while_playing_transposes");
            {
                auto p = prepared();
                juce::MidiBuffer on;
                on.addEvent(juce::MidiMessage::noteOn(1, 36, 0.8f), 0);
                const auto r = render(*p, 0.5, 48000.0, 480, &on);
                expect(rms(r.left, 4800, r.left.size()) > 0.01, "the middle C sounds");
                juce::MidiBuffer off;
                off.addEvent(juce::MidiMessage::noteOff(1, 36), 0);
                render(*p, 0.1, 48000.0, 480, &off);
                expectEquals(p->soundingKey(), -1);
                p->setPlaying(true);
                render(*p, 0.1);
                juce::MidiBuffer up;
                up.addEvent(juce::MidiMessage::noteOn(1, 65, 0.8f), 0);
                render(*p, 0.1, 48000.0, 480, &up);
                expectEquals(p->transposition(), 5);
            }

            beginTest("its_state_reads_back_and_undo_steps_back");
            {
                auto p = prepared();
                p->edit([](Project& pr) { pr.bpm = 140.0; pr.panel.cutoff = 0.1f; });
                juce::MemoryBlock state;
                p->getStateInformation(state);
                auto q = prepared();
                q->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
                expect(q->project() == p->project());
                expect(! q->canUndo(), "an opened project starts its own history");
                p->undo();
                expectEquals(p->project().bpm, 128.0);
                p->redo();
                expectEquals(p->project().bpm, 140.0);
            }

            beginTest("a_knob_turned_while_recording_is_written_to_the_pattern");
            {
                auto p = prepared();
                p->setPlaying(true);
                render(*p, 0.3);
                p->setRecording(true);
                const int step = p->playingStep();
                p->knobMoved(Knob::decay, 0.77f);
                const auto& pat = p->project().pattern(p->playingPattern(), p->playingVariationB());
                expectWithinAbsoluteError(pat.motion[static_cast<size_t>(Knob::decay)][static_cast<size_t>(step)], 0.77f, 1e-6f);
                expectWithinAbsoluteError(p->project().panel.decay, 0.77f, 1e-6f);
            }

            // COOLSYNTH303_RENDER=<folder>: the new project's chain once through and its first line with the knobs moved,
            // as WAV files, for a listening pass
            if (const auto folder = juce::SystemStats::getEnvironmentVariable("COOLSYNTH303_RENDER", {}); folder.isNotEmpty())
            {
                beginTest("listening_files");
                const juce::File dir(folder);
                dir.createDirectory();
                const auto write = [&](const juce::String& fileName, const std::vector<float>& left) {
                    auto file = dir.getChildFile(fileName + ".wav");
                    file.deleteFile();
                    juce::WavAudioFormat wav;
                    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream>(file);
                    auto w = wav.createWriterFor(stream, juce::AudioFormatWriterOptions {}.withSampleRate(48000.0).withNumChannels(2).withBitsPerSample(24));
                    juce::AudioBuffer<float> b(2, static_cast<int>(left.size()));
                    for (int ch = 0; ch < 2; ++ch)
                        b.copyFrom(ch, 0, left.data(), static_cast<int>(left.size()));
                    w->writeFromAudioSampleBuffer(b, 0, b.getNumSamples());
                };
                {
                    auto p = prepared();
                    p->edit([](Project& pr) { pr.chainOn = true; pr.loop = false; }, false);
                    p->setPlaying(true);
                    write("chain", render(*p, 12.0).left);
                }
                {
                    auto p = prepared();
                    p->edit([](Project& pr) { pr.panel.square = true; pr.panel.resonance = 0.9f; pr.panel.envMod = 0.8f; pr.panel.decay = 0.3f; pr.panel.drive = 0.6f; }, false);
                    p->setPlaying(true);
                    write("line1-square-resonant", render(*p, 8.0).left);
                }
                {
                    auto p = prepared();
                    p->edit([](Project& pr) { pr.panel.driveOn = false; pr.panel.delayOn = false; }, false);
                    p->setPlaying(true);
                    write("line1-dry", render(*p, 8.0).left);
                }
                {
                    auto p = prepared();
                    p->edit([](Project& pr) { pr.panel.driveOn = false; pr.panel.delayOn = false; pr.panel.devilFish = true; }, false);
                    p->setPlaying(true);
                    write("line1-devil-fish-stock-settings", render(*p, 8.0).left);
                }
                {
                    auto p = prepared();
                    p->edit([](Project& pr) {
                        auto& pn = pr.panel;
                        pn.driveOn = false;
                        pn.delayOn = false;
                        pn.devilFish = true;
                        pn.overdrive = 0.5f;
                        pn.slideTime = 0.5f;
                        pn.normalDecay = 0.35f;
                        pn.accentDecay = 0.2f;
                        pn.tracking = 0.4f;
                        pn.filterFm = 0.25f;
                        pn.muffler = 1;
                        pn.resonance = 0.9f;
                        pn.envMod = 0.45f;
                    }, false);
                    p->setPlaying(true);
                    write("line1-devil-fish-pushed", render(*p, 8.0).left);
                }
                expect(true);
            }

            beginTest("a_chain_once_through_stops_the_transport_at_its_end");
            {
                auto p = prepared();
                p->edit([](Project& pr) {
                    pr.chain = { { 0, false, 1 } };
                    pr.chainOn = true;
                    pr.loop = false;
                    pr.bpm = 240.0;
                }, false);
                p->setPlaying(true);
                render(*p, 1.2); // sixteen steps at 240 BPM take a second
                expect(! p->isPlaying());
            }
        }
    };

    static ProcessorTests processorTests;
}
