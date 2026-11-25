// The beat sync in the 303 (CoolSynth's D361): its two switches kept in the settings, the SYNC key's word, click and
// hint, Options' Play and Stop switch, the processor following the sync through its play head, alone and with another
// peer over the loopback (real sockets: COOLSYNTH_SKIP_NETWORK_TESTS=1 skips those, and so does another program on
// this computer that speaks the beat sync), incoming MIDI clock ignored while others are in the session and followed
// while alone, and get_sound telling what the transport does. With COOLSYNTH303_SNAPSHOTS=<folder> it also writes the
// panel with its SYNC key in every theme, and the MIDI page. At 48 kHz and 120 BPM a step is 6000 samples; a block
// here is 480 samples, 10 ms. Classes of their own, every object on the heap: the MSVC Debug runner's stack is small.

#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Editor.h"
#include "Processor.h"
#include "mcp/Tools303.h"
#include "sync/BeatSync.h"
#include "sync/SyncClock.h"
#include "ui/OptionsWindow.h"
#include "ui/Panel.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

namespace
{
    using namespace cs303;

    constexpr double kRate = 48000.0;
    constexpr int kBlock = 480;
    constexpr std::int64_t kBlockMicros = 10000;
    constexpr double kMicrosPerSample = 1.0e6 / kRate;

    bool networkTestsSkipped()
    {
        return juce::SystemStats::getEnvironmentVariable("COOLSYNTH_SKIP_NETWORK_TESTS", {}).getIntValue() != 0;
    }

    double waitFor(double limitSeconds, const std::function<bool()>& condition)
    {
        const auto start = std::chrono::steady_clock::now();
        for (;;)
        {
            const double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            if (condition())
                return t;
            if (t > limitSeconds)
                return -1.0;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    /** A lone peer listens for 600 ms: whether another program on this computer speaks the beat sync (it would join
        the tests' sessions and change their counts). -1 if no loopback gateway could open. */
    int otherPeersAround()
    {
        coolsynth::sync::BeatSync::Options options;
        options.loopbackOnly = true;
        auto probe = std::make_unique<coolsynth::sync::BeatSync>(120.0, options);
        probe->setEnabled(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        const auto status = probe->status();
        return status.gateways == 0 ? -1 : status.peers;
    }

    /** A processor at Light (it renders well ahead of real time) at 120 BPM, with a beat sync of its own. */
    std::unique_ptr<Processor> syncedProcessor(bool network, const juce::File& settings = {})
    {
        auto p = std::make_unique<Processor>();
        p->setPlayConfigDetails(0, 2, kRate, kBlock);
        p->setAccuracy(1);
        p->prepareToPlay(kRate, kBlock);
        p->edit([](Project& pr) { pr.bpm = 120.0; }, false);
        p->useBeatSyncForTesting(network, settings);
        return p;
    }

    /** Drives the processor block after block on a clock of its own (block i heard at origin + i * 10 ms), as a device
        would, and a peer's audio side at the same moments; keeps every step's sample and the MIDI that came out. */
    struct Rig
    {
        Processor& p;
        coolsynth::sync::BeatSync* peer = nullptr;
        std::int64_t origin = 0, blocks = 0, now = 0;
        std::vector<double> steps;
        std::vector<coolsynth::sync::BlockBeats> peerBlocks;
        std::vector<std::pair<std::int64_t, juce::MidiMessage>> midiOut;
        std::function<void(juce::MidiBuffer&, std::int64_t, int)> in;
        juce::AudioBuffer<float> buffer { 2, kBlock };

        Rig(Processor& processor, std::int64_t start) : p(processor), origin(start)
        {
            p.setHostClockForTesting([this] { return now; });
            p.logStepsForTesting(&steps);
        }

        ~Rig()
        {
            p.setHostClockForTesting({});
            p.logStepsForTesting(nullptr);
        }

        void step()
        {
            now = origin + blocks * kBlockMicros;
            juce::MidiBuffer midi;
            if (in)
                in(midi, blocks * kBlock, kBlock);
            buffer.clear();
            p.processBlock(buffer, midi);
            for (const auto m : midi)
                midiOut.emplace_back(blocks * kBlock + m.samplePosition, m.getMessage());
            peerBlocks.push_back(peer != nullptr ? peer->audio().beginBlock(now, kBlock, kMicrosPerSample) : coolsynth::sync::BlockBeats {});
            ++blocks;
        }

        void run(int count)
        {
            for (int i = 0; i < count; ++i)
                step();
        }

        /** The blocks due by now on the real clock (the network's tests). */
        void catchUp()
        {
            const std::int64_t due = (coolsynth::sync::hostMicrosNow() - origin) / kBlockMicros;
            while (blocks < due)
                step();
        }

        double until(double limitSeconds, const std::function<bool()>& condition)
        {
            return waitFor(limitSeconds, [&] { catchUp(); return condition(); });
        }

        void runFor(double seconds) { until(seconds, [] { return false; }); }

        double peerBeatAt(double sample) const
        {
            const auto block = static_cast<std::size_t>(sample / kBlock);
            const auto& b = peerBlocks[std::min(block, peerBlocks.size() - 1)];
            return b.beat + (sample - static_cast<double>(block * kBlock)) * b.beatsPerSample;
        }

        /** How far a sample is from the peer's grid of sixteenths, in samples. */
        double offGrid(double sample) const
        {
            const auto& b = peerBlocks[std::min(static_cast<std::size_t>(sample / kBlock), peerBlocks.size() - 1)];
            const double beat = peerBeatAt(sample);
            return (beat - std::round(beat * 4.0) / 4.0) / b.beatsPerSample;
        }

        /** How far a sample is from the peer's nearest bar line, in samples. */
        double offBar(double sample) const
        {
            const auto& b = peerBlocks[std::min(static_cast<std::size_t>(sample / kBlock), peerBlocks.size() - 1)];
            const double beat = peerBeatAt(sample);
            return (beat - 4.0 * std::round(beat / 4.0)) / b.beatsPerSample;
        }
    };

    /** A MIDI clock coming in at this tempo, a start first. */
    std::function<void(juce::MidiBuffer&, std::int64_t, int)> incomingClock(double bpm)
    {
        auto next = std::make_shared<double>(-1.0);
        const double interval = kRate * 60.0 / bpm / 24.0;
        return [next, interval](juce::MidiBuffer& midi, std::int64_t pos, int n)
        {
            if (*next < 0.0)
            {
                midi.addEvent(juce::MidiMessage::midiStart(), 0);
                *next = static_cast<double>(pos);
            }
            for (; *next < static_cast<double>(pos + n); *next += interval)
                midi.addEvent(juce::MidiMessage::midiClock(), juce::jlimit(0, n - 1, static_cast<int>(std::llround(*next)) - static_cast<int>(pos)));
        };
    }

    /** get_sound's answer, as Claude reads it. */
    juce::var getSound(Processor& p)
    {
        const coolsynth::mcp::Server::Runner direct = [](std::function<void()> job) { job(); };
        for (const auto& t : tools303(p, direct))
            if (t.name == "get_sound")
                return juce::JSON::parse(t.call(juce::var(new juce::DynamicObject()))["content"][0]["text"].toString());
        return {};
    }

    void writePng(const juce::Image& image, const juce::File& file)
    {
        file.deleteFile();
        juce::FileOutputStream out(file);
        juce::PNGImageFormat().writeImageToStream(image, out);
    }

    juce::Component* childWithId(juce::Component& root, const juce::String& id)
    {
        for (auto* child : root.getChildren())
            if (child->getComponentID() == id)
                return child;
        return nullptr;
    }

    // =================================================================================================================

    class BeatSyncChoiceTests final : public juce::UnitTest
    {
    public:
        BeatSyncChoiceTests() : juce::UnitTest("Beat sync choice and key", "CoolSynth303") {}

        void runTest() override
        {
            beginTest("on_and_shared_until_switched_off_and_kept");
            {
                auto empty = std::make_unique<juce::PropertySet>();
                const auto first = syncChoiceFrom(*empty);
                expect(first.on && first.sharing, "both on by default");
                keepSyncChoice(*empty, { false, false });
                const auto kept = syncChoiceFrom(*empty);
                expect(! kept.on && ! kept.sharing, "both off, read back");
            }

            beginTest("the_sync_keys_word_counts_the_other_peers");
            {
                expectEquals(syncKeyText(0), juce::String("Sync"));
                expectEquals(syncKeyText(1), juce::String("Sync 1"));
                expectEquals(syncKeyText(2), juce::String("Sync 2"));
            }

            beginTest("no_beat_sync_without_one_and_none_in_an_export");
            {
                auto plain = std::make_unique<Processor>();
                expect(! plain->hasBeatSync(), "a processor in the tests (as a plugin's) has none of its own");
                auto offline = std::make_unique<Processor>(true);
                offline->useBeatSyncForTesting(false);
                expect(! offline->hasBeatSync(), "an export's processor never makes one");
                auto e = std::make_unique<Editor>(*plain);
                expect(e->topBarForTesting().syncKeyForTesting() == nullptr, "no SYNC key without a beat sync");
                expect(! e->optionsHooks().syncSharing, "nor Options' switch");
            }

            beginTest("the_sync_key_switches_it_and_the_settings_keep_it");
            {
                const auto file = juce::File::createTempFile(".settings");
                const auto read = [&]
                {
                    auto settings = std::make_unique<juce::PropertiesFile>(file, Processor::settingsOptions());
                    return syncChoiceFrom(*settings);
                };
                auto p = syncedProcessor(false, file);
                auto e = std::make_unique<Editor>(*p);
                e->setSize(Editor::baseWidth, Editor::baseHeight);
                auto* key = e->topBarForTesting().syncKeyForTesting();
                expect(key != nullptr, "the standalone's top bar has a SYNC key");
                if (key != nullptr)
                {
                    expect(key->isOn() && key->textOf() == "Sync", "lit, alone: SYNC");
                    expect(key->getHelpText().contains("beat sync"), "with a hint on what it does: " + key->getHelpText());
                    expect(e->hintAt(key->getBounds().getCentre()).contains("beat sync"), "the hint line says it under the mouse");
                    expect(! key->getBounds().isEmpty() && ! key->getBounds().intersects(e->topBarForTesting().playKeyForTesting().getBounds()),
                           "placed, clear of Play");
                    key->onClick();
                    expect(! p->beatSyncOn() && ! key->isOn(), "a click switches it off");
                    expect(! read().on, "kept off in the settings");
                    key->onClick();
                    expect(p->beatSyncOn() && read().on, "and on again, kept");
                }

                // Options, MIDI: Play and Stop shared, a switch of its own
                auto devices = std::make_unique<juce::AudioDeviceManager>();
                auto panel = ui::makeOptionsPanelForTesting(e->optionsHooks(), devices.get());
                ui::showOptionsPageForTesting(*panel, 1);
                expectEquals(ui::optionsPageNameForTesting(*panel), juce::String("MIDI"));
                auto* sharing = dynamic_cast<ui::Key*>(childWithId(*panel, "syncSharing"));
                expect(sharing != nullptr && sharing->isVisible(), "the MIDI page's Share Play and Stop");
                if (sharing != nullptr)
                {
                    expect(panel->getLocalBounds().contains(sharing->getBounds()), "inside the page");
                    expect(sharing->isOn(), "on by default");
                    sharing->onClick();
                    expect(! p->startStopSharing() && ! sharing->isOn(), "a click: not shared");
                    expect(read().on && ! read().sharing, "on, Play and Stop not shared, kept");
                }

                const auto folder = juce::SystemStats::getEnvironmentVariable("COOLSYNTH303_SNAPSHOTS", {});
                if (folder.isNotEmpty())
                {
                    const juce::File dir(folder);
                    dir.createDirectory();
                    writePng(panel->createComponentSnapshot(panel->getLocalBounds(), true, 2.0f), dir.getChildFile("options-midi-sync.png"));
                    // the panel with its SYNC key in every theme, then back to the one in use
                    const int themeBefore = ui::currentTheme();
                    const auto names = ui::themeNames();
                    for (int t = 0; t < names.size(); ++t)
                    {
                        e->chooseThemeForTesting(t);
                        e->layOut();
                        e->refreshAll(true);
                        writePng(e->board().createComponentSnapshot(e->board().getLocalBounds(), true, 2.0f),
                                 dir.getChildFile("sync-panel-" + names[t].toLowerCase() + ".png"));
                    }
                    e->chooseThemeForTesting(themeBefore);
                }
                panel.reset(); // before the device manager it listens to
                devices.reset();
                e.reset();
                p.reset();
                file.deleteFile();
            }
        }
    };

    class BeatSyncAloneTests final : public juce::UnitTest
    {
    public:
        BeatSyncAloneTests() : juce::UnitTest("Beat sync alone", "CoolSynth303") {}

        void runTest() override
        {
            beginTest("alone_play_starts_at_once_and_the_steps_and_clock_keep_the_grid");
            {
                auto p = syncedProcessor(false);
                auto rig = std::make_unique<Rig>(*p, 5'000'000'000);
                auto& r = *rig;
                r.run(10);
                expect(! p->isPlaying() && r.steps.empty(), "stopped, nothing plays");
                p->setPlaying(true);
                const auto pressed = static_cast<double>(r.blocks * kBlock);
                r.run(1);
                expect(p->isPlaying() && ! p->countingIn() && ! p->isFollowingSync(), "alone, Play plays at once");
                r.run(200); // two seconds: 16 steps
                expectGreaterOrEqual(static_cast<int>(r.steps.size()), 16);
                if (! r.steps.empty())
                {
                    expectWithinAbsoluteError(r.steps.front(), pressed, 0.5, "the first step at the press");
                    double worst = 0.0;
                    for (std::size_t k = 0; k < r.steps.size(); ++k)
                        worst = std::max(worst, std::abs(r.steps[k] - (pressed + static_cast<double>(k) * 6000.0)));
                    expectLessOrEqual(worst, 1.0, "every step on the grid, within a sample");
                }
                int clocks = 0, starts = 0, worstClock = 0;
                for (const auto& [at, m] : r.midiOut)
                {
                    starts += m.isMidiStart() ? 1 : 0;
                    if (m.isMidiClock())
                    {
                        ++clocks;
                        const auto from = at - static_cast<std::int64_t>(pressed);
                        worstClock = std::max(worstClock, static_cast<int>(std::abs(from - std::llround(static_cast<double>(from) / 1000.0) * 1000)));
                    }
                }
                expectEquals(starts, 1, "one MIDI start");
                expectGreaterOrEqual(clocks, 95, "its MIDI clock out goes on while the sync leads");
                expectLessOrEqual(worstClock, 1, "24 a beat, on the grid");
                const auto sound = getSound(*p);
                expect(static_cast<bool>(sound["playing"]) && sound["leads"].toString() == "panel", "get_sound: playing, by its own Play");
                expect(static_cast<bool>(sound["sync"]["on"]) && static_cast<int>(sound["sync"]["peers"]) == 0 && ! static_cast<bool>(sound["sync"]["leads"]),
                       "get_sound: the sync on, alone");

                // the panel's tempo goes to the sync
                p->edit([](Project& pr) { pr.bpm = 125.0; }, false);
                r.run(2);
                expectWithinAbsoluteError(p->tempoNow(), 125.0, 1.0e-6, "the panel's tempo is the sync's");
                const auto before = r.steps.size();
                r.run(100);
                if (r.steps.size() > before + 2)
                    expectWithinAbsoluteError(r.steps.back() - r.steps[r.steps.size() - 2], 5760.0, 1.0, "and plays at it");

                p->setPlaying(false);
                r.run(2);
                expect(! p->isPlaying(), "Stop through the sync");
            }

            beginTest("alone_an_incoming_midi_clock_leads_as_before_and_get_sound_says_so");
            {
                auto p = syncedProcessor(false);
                auto rig = std::make_unique<Rig>(*p, 5'000'000'000);
                auto& r = *rig;
                r.in = incomingClock(100.0);
                r.run(100);
                expect(p->isFollowingClock() && ! p->isFollowingSync(), "the clock leads while the sync is alone");
                expect(p->isPlaying(), "its start started it");
                expectWithinAbsoluteError(p->tempoNow(), 100.0, 1.5);
                const auto sound = getSound(*p);
                expect(static_cast<bool>(sound["playing"]), "get_sound: playing while it follows a clock (D359's aside)");
                expectEquals(sound["leads"].toString(), juce::String("clock"));
                expectWithinAbsoluteError(static_cast<double>(sound["tempoNow"]), 100.0, 1.5, "at the clock's tempo");
            }

            beginTest("set_values_switches_the_sync_and_its_sharing");
            {
                auto p = syncedProcessor(false);
                const coolsynth::mcp::Server::Runner direct = [](std::function<void()> job) { job(); };
                const auto tools = tools303(*p, direct);
                for (const auto& t : tools)
                    if (t.name == "set_values")
                    {
                        const auto undoBefore = p->canUndo();
                        t.call(juce::JSON::parse("{\"values\": {\"sync\": false, \"syncSharing\": false}}"));
                        expect(! p->beatSyncOn() && ! p->startStopSharing(), "both off");
                        expect(p->canUndo() == undoBefore, "no undo step for the switches alone");
                        t.call(juce::JSON::parse("{\"values\": {\"sync\": true}}"));
                        expect(p->beatSyncOn(), "on again");
                    }
                const auto sound = getSound(*p);
                expect(static_cast<bool>(sound["sync"]["on"]) && ! static_cast<bool>(sound["sync"]["sharing"]), "get_sound reads them back");
            }
        }
    };

    class BeatSyncNetworkTests final : public juce::UnitTest
    {
    public:
        BeatSyncNetworkTests() : juce::UnitTest("Beat sync network with a peer", "CoolSynth303") {}

        /** False, with a line in the log, when the network tests cannot run as they assume. */
        bool networkReady()
        {
            if (networkTestsSkipped())
            {
                logMessage("skipped: COOLSYNTH_SKIP_NETWORK_TESTS");
                return false;
            }
            const int others = otherPeersAround();
            if (others < 0)
            {
                logMessage("skipped: no loopback gateway could open (no multicast on the loopback interface?)");
                return false;
            }
            if (others > 0)
            {
                logMessage("another program speaks the beat sync on this computer; skipped");
                return false;
            }
            return true;
        }

        void runTest() override
        {
            beginTest("the_303_plays_with_another_peer_on_the_loopback");
            if (networkReady())
                withAPeer();
            else
                expect(true);

            beginTest("joining_an_older_session_the_303_takes_its_tempo");
            if (networkReady())
                joiningAnOlderSession();
            else
                expect(true);
        }

    private:
        void withAPeer()
        {
            auto p = syncedProcessor(true);
            coolsynth::sync::BeatSync::Options options;
            options.loopbackOnly = true;
            auto peer = std::make_unique<coolsynth::sync::BeatSync>(97.0, options);
            auto rig = std::make_unique<Rig>(*p, coolsynth::sync::hostMicrosNow());
            auto& r = *rig;
            r.peer = peer.get();
            r.runFor(1.0); // the 303's session is the older
            peer->setEnabled(true);
            const double met = r.until(3.0, [&] { return p->syncPeers() == 1 && peer->peerCount() == 1; });
            logMessage("met in " + juce::String(met * 1000.0, 1) + " ms");
            expect(met >= 0.0, "the two meet");
            r.runFor(0.1);
            expectWithinAbsoluteError(r.peerBlocks.back().bpm, 120.0, 1.0e-6, "the peer takes the 303's tempo");
            expect(p->isFollowingSync(), "with a peer the sync leads");
            {
                auto e = std::make_unique<Editor>(*p);
                auto* key = e->topBarForTesting().syncKeyForTesting();
                expect(key != nullptr && key->textOf() == "Sync 1", "the SYNC key counts the other peer");
                const auto folder = juce::SystemStats::getEnvironmentVariable("COOLSYNTH303_SNAPSHOTS", {});
                if (folder.isNotEmpty())
                {
                    e->layOut();
                    e->refreshAll(true);
                    writePng(e->board().createComponentSnapshot({ 0, 0, Editor::baseWidth, 42 }, true, 2.0f),
                             juce::File(folder).getChildFile("sync-topbar-with-a-peer.png"));
                }
            }

            // An incoming clock is not followed while the sync has a peer.
            r.in = incomingClock(100.0);
            r.runFor(0.5);
            expect(! p->isFollowingClock() && p->isFollowingSync() && ! p->isPlaying(), "a MIDI clock is ignored, its start too");
            r.in = {};
            r.runFor(0.6);

            // The peer plays: the 303 counts in and starts on the session's bar line, its steps on the peer's grid.
            r.catchUp();
            r.steps.clear();
            peer->audio().play(0);
            const double countIn = r.until(1.0, [&] { return p->countingIn() || p->isPlaying(); });
            expect(countIn >= 0.0, "the peer's Play reaches the 303");
            const double started = r.until(3.0, [&] { return p->isPlaying(); });
            expect(started >= 0.0, "it starts on the next bar line");
            r.runFor(1.5);
            if (! r.steps.empty())
            {
                const double offBar = r.offBar(r.steps.front());
                logMessage("the 303's first step: " + juce::String(offBar, 3) + " samples off the peer's bar line, its beat "
                           + juce::String(r.peerBeatAt(r.steps.front()), 4));
                expectLessOrEqual(std::abs(offBar), 1.5, "its first step on the peer's bar line");
            }
            expect(r.peerBlocks.back().playing, "the peer plays");
            expectGreaterOrEqual(static_cast<int>(r.steps.size()), 10);
            double worst = 0.0;
            for (const auto s : r.steps)
                worst = std::max(worst, std::abs(r.offGrid(s)));
            logMessage("steps off the peer's grid by " + juce::String(worst, 3) + " samples at most; behind real time by "
                       + juce::String(static_cast<double>(coolsynth::sync::hostMicrosNow() - r.now) / 1000.0, 1) + " ms");
            expectLessOrEqual(worst, 1.5, "every step on the peer's grid, within a sample or so");
            {
                const auto sound = getSound(*p);
                expect(static_cast<bool>(sound["playing"]) && sound["leads"].toString() == "sync", "get_sound: playing, led by the sync");
                expect(static_cast<int>(sound["sync"]["peers"]) == 1 && static_cast<bool>(sound["sync"]["leads"]), "get_sound: one peer, the sync leads");
            }

            // The peer's tempo: the 303 plays at it, and its panel takes it, without an undo step.
            r.catchUp();
            const bool couldUndo = p->canUndo();
            peer->audio().setTempo(133.5);
            const double took = r.until(1.0, [&] { return coolsynth::sync::tempoFromBpm(p->tempoNow()) == coolsynth::sync::tempoFromBpm(133.5); });
            expect(took >= 0.0 && took < 0.2, "the peer's tempo reaches the 303");
            p->syncTick();
            expectWithinAbsoluteError(p->project().bpm, 133.5, 1.0e-3, "and becomes the panel's");
            expect(p->canUndo() == couldUndo, "with no undo step");
            r.steps.clear();
            r.runFor(1.0);
            worst = 0.0;
            for (const auto s : r.steps)
                worst = std::max(worst, std::abs(r.offGrid(s)));
            expectLessOrEqual(worst, 1.5, "on the grid at the new tempo");

            // Stop and Play both ways, shared.
            p->setPlaying(false);
            const double stopped = r.until(1.0, [&] { return ! p->isPlaying() && ! r.peerBlocks.back().playing; });
            expect(stopped >= 0.0 && stopped < 0.2, "the 303's Stop stops the peer");
            r.steps.clear();
            p->setPlaying(true);
            const double bothPlay = r.until(3.0, [&] { return p->isPlaying() && r.peerBlocks.back().playing; });
            expect(bothPlay >= 0.0, "the 303's Play starts the peer");
            r.runFor(0.3);
            expect(! r.steps.empty(), "and plays");
            if (! r.steps.empty())
            {
                const double offBar = r.offBar(r.steps.front());
                logMessage("the 303's first step after its Play: " + juce::String(offBar, 3) + " samples off the peer's bar line");
                expectLessOrEqual(std::abs(offBar), 1.5, "its first step on the peer's bar line");
            }
            r.catchUp();
            peer->audio().stop(0);
            const double peerStopped = r.until(1.0, [&] { return ! p->isPlaying(); });
            expect(peerStopped >= 0.0 && peerStopped < 0.2, "the peer's Stop stops the 303");

            // The 303's tempo to the peer.
            p->edit([](Project& pr) { pr.bpm = 110.0; }, false);
            const double sent = r.until(1.0, [&] { return coolsynth::sync::tempoFromBpm(r.peerBlocks.back().bpm) == coolsynth::sync::tempoFromBpm(110.0); });
            expect(sent >= 0.0 && sent < 0.2, "the panel's tempo reaches the peer");

            // The peer leaves: alone, an incoming clock leads again.
            peer->setEnabled(false);
            const double alone = r.until(1.0, [&] { return p->syncPeers() == 0; });
            expect(alone >= 0.0, "alone again");
            r.in = incomingClock(100.0);
            r.runFor(0.5);
            expect(p->isFollowingClock() && ! p->isFollowingSync(), "alone, the MIDI clock is followed");
            r.in = {};
            r.peer = nullptr;
            rig.reset();
            p.reset();
        }

        void joiningAnOlderSession()
        {
            // The peer's session is the older; the 303's audio starts only once they have met (a device opened late):
            // its panel's tempo must not win over the session's.
            coolsynth::sync::BeatSync::Options options;
            options.loopbackOnly = true;
            auto older = std::make_unique<coolsynth::sync::BeatSync>(97.0, options);
            older->setEnabled(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
            auto q = syncedProcessor(true);
            const double joined = waitFor(3.0, [&] { return q->syncPeers() == 1; });
            expect(joined >= 0.0, "the 303 joins the older session");
            auto lateRig = std::make_unique<Rig>(*q, coolsynth::sync::hostMicrosNow());
            lateRig->peer = older.get();
            lateRig->runFor(0.5);
            q->syncTick();
            expectWithinAbsoluteError(lateRig->peerBlocks.back().bpm, 97.0, 1.0e-3, "the session keeps its tempo");
            expectWithinAbsoluteError(q->tempoNow(), 97.0, 1.0e-3, "the 303 plays at it");
            expectWithinAbsoluteError(q->project().bpm, 97.0, 1.0e-3, "and its panel takes it");
            lateRig->peer = nullptr;
            lateRig.reset();
            q.reset();
        }
    };

    BeatSyncChoiceTests beatSyncChoiceTests;
    BeatSyncAloneTests beatSyncAloneTests;
    BeatSyncNetworkTests beatSyncNetworkTests;
}
