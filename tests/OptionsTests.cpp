// The options' own window (D349): its pages build in the standalone's form (with a device manager) and a plugin's
// (without), every part inside the panel, and the Accuracy keys set the processor's accuracy. With
// COOLSYNTH303_SNAPSHOTS=<folder> it also writes each page and the top bar with its Options key. A class of its own:
// MSVC's Debug frames overflow with many processors and editors in one runTest.

#include <map>
#include <memory>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Editor.h"
#include "Processor.h"
#include "ui/OptionsWindow.h"
#include "ui/Theme.h"

namespace
{
    using namespace cs303;

    juce::Component* childWithId(juce::Component& root, const juce::String& id)
    {
        for (auto* child : root.getChildren())
            if (child->getComponentID() == id)
                return child;
        return nullptr;
    }

    void press(juce::Component& target)
    {
        const auto at = target.getLocalBounds().getCentre().toFloat();
        const auto now = juce::Time::getCurrentTime();
        const juce::MouseEvent e(juce::Desktop::getInstance().getMainMouseSource(), at, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),
                                 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &target, &target, now, at, now, 1, false);
        target.mouseDown(e);
        target.mouseUp(e);
    }

    void writePng(const juce::Image& image, const juce::File& file)
    {
        file.deleteFile();
        juce::FileOutputStream out(file);
        juce::PNGImageFormat().writeImageToStream(image, out);
    }

    class OptionsTests final : public juce::UnitTest
    {
    public:
        OptionsTests() : juce::UnitTest("Options", "CoolSynth303") {}

        void runTest() override
        {
            auto p = std::make_unique<Processor>();
            p->setPlayConfigDetails(0, 2, 48000.0, 480);
            p->prepareToPlay(48000.0, 480);
            auto e = std::make_unique<Editor>(*p);
            e->setSize(Editor::baseWidth, Editor::baseHeight);
            const auto folder = juce::SystemStats::getEnvironmentVariable("COOLSYNTH303_SNAPSHOTS", {});
            const juce::File dir(folder);
            if (folder.isNotEmpty())
                dir.createDirectory();

            // The standalone's pages need its device manager; one not opened is enough to build them.
            auto devices = std::make_unique<juce::AudioDeviceManager>();
            for (const bool standalone : { true, false })
            {
                beginTest(standalone ? "the_standalone_pages_build_and_set_the_accuracy" : "the_plugin_pages_build_and_set_the_accuracy");
                auto panel = ui::makeOptionsPanelForTesting(e->optionsHooks(), standalone ? devices.get() : nullptr);
                const juce::StringArray expected = standalone ? juce::StringArray { "Audio", "MIDI", "Panel", "Export" }
                                                              : juce::StringArray { "Host", "Panel", "Export" };
                const int pages = ui::showOptionsPageForTesting(*panel, 0);
                expectEquals(pages, expected.size());
                for (int page = 0; page < pages; ++page)
                {
                    ui::showOptionsPageForTesting(*panel, page);
                    const auto shown = ui::optionsPageNameForTesting(*panel);
                    expectEquals(shown, expected[page]);
                    for (auto* child : panel->getChildren())
                        if (child->isVisible())
                            expect(panel->getLocalBounds().contains(child->getBounds()), shown + ": a part outside the panel");

                    auto* best = childWithId(*panel, "accuracy0");
                    auto* light = childWithId(*panel, "accuracy1");
                    const bool hasAccuracy = shown == "Audio" || shown == "Host";
                    expect(best != nullptr && light != nullptr);
                    if (best == nullptr || light == nullptr)
                        continue;
                    expect(best->isVisible() == hasAccuracy && light->isVisible() == hasAccuracy, shown + ": the accuracy row only on its page");
                    if (hasAccuracy)
                    {
                        press(*light);
                        expectEquals(p->accuracy(), 1, "Light");
                        if (folder.isNotEmpty())
                            writePng(panel->createComponentSnapshot(panel->getLocalBounds(), true, 2.0f),
                                     dir.getChildFile("options-" + shown.toLowerCase() + "-light.png"));
                        press(*best);
                        expectEquals(p->accuracy(), 0, "Best");
                    }
                    if (folder.isNotEmpty())
                        writePng(panel->createComponentSnapshot(panel->getLocalBounds(), true, 2.0f),
                                 dir.getChildFile("options-" + shown.toLowerCase() + ".png"));
                }
                panel.reset(); // before the device manager it listens to
            }

            beginTest("the_window_size_follows_the_options");
            {
                auto hooks = e->optionsHooks();
                expectEquals(hooks.windowSize(), 100);
                hooks.chooseWindowSize(75);
                expectEquals(e->getWidth(), Editor::baseWidth * 3 / 4);
                expectEquals(hooks.windowSize(), 75);
                hooks.chooseWindowSize(100);
                expectEquals(e->getWidth(), Editor::baseWidth);
                expect(! e->optionsOpen());
            }

            if (folder.isNotEmpty())
            {
                beginTest("snapshot_of_the_top_bar");
                e->layOut();
                e->refreshAll();
                writePng(e->board().createComponentSnapshot({ 0, 0, Editor::baseWidth, 42 }, true, 2.0f), dir.getChildFile("topbar.png"));
                expect(true);

                // the Audio and Panel pages in every theme (D352), then back to the one in use
                beginTest("snapshots_of_the_pages_in_every_theme");
                const int themeBefore = ui::currentTheme();
                const auto names = ui::themeNames();
                for (int t = 0; t < names.size(); ++t)
                {
                    e->chooseThemeForTesting(t);
                    auto panel = ui::makeOptionsPanelForTesting(e->optionsHooks(), devices.get());
                    for (const int page : { 0, 2 })
                    {
                        ui::showOptionsPageForTesting(*panel, page);
                        writePng(panel->createComponentSnapshot(panel->getLocalBounds(), true, 2.0f),
                                 dir.getChildFile("options-" + ui::optionsPageNameForTesting(*panel).toLowerCase() + "-" + names[t].toLowerCase() + ".png"));
                    }
                }
                e->chooseThemeForTesting(themeBefore);
                expect(true);
            }

            devices.reset();
            e.reset();
            p.reset();
        }
    };

    static OptionsTests optionsTests;

    // The Export page (D355): its keys make the job and are kept for next time, Export writes a 24-bit WAV and the
    // dry file beside it, and a cancelled or abandoned export leaves no file. With COOLSYNTH303_SNAPSHOTS it writes
    // the page in Graphite and Silver, idle, rendering and written. A class of its own, its parts on the heap.
    class OptionsExportTests final : public juce::UnitTest
    {
    public:
        OptionsExportTests() : juce::UnitTest("Options Export page", "CoolSynth303") {}

        void runTest() override
        {
            auto p = std::make_unique<Processor>();
            p->setPlayConfigDetails(0, 2, 48000.0, 480);
            p->prepareToPlay(48000.0, 480);
            auto e = std::make_unique<Editor>(*p);
            e->setSize(Editor::baseWidth, Editor::baseHeight);
            e->choosePattern(2, true);
            const auto snapshots = juce::SystemStats::getEnvironmentVariable("COOLSYNTH303_SNAPSHOTS", {});
            const juce::File dir(snapshots);
            const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("CoolSynth303ExportPageTest");
            (void) folder.deleteRecursively();
            folder.createDirectory();

            // the choices kept in a map here, not in the app's settings
            auto store = std::make_shared<std::map<juce::String, juce::var>>();
            auto hooks = e->optionsHooks();
            hooks.kept = [store](const juce::String& key) { const auto f = store->find(key); return f != store->end() ? f->second : juce::var(); };
            hooks.keep = [store](const juce::NamedValueSet& values) { for (const auto& v : values) (*store)[v.name.toString()] = v.value; };
            const auto exportPage = [&]
            {
                auto panel = ui::makeOptionsPanelForTesting(hooks, nullptr);
                ui::showOptionsPageForTesting(*panel, 2);
                return panel;
            };
            const auto choose = [&](juce::Component& panel, const juce::String& id)
            {
                auto* key = childWithId(panel, id);
                expect(key != nullptr && key->isVisible(), id);
                if (key != nullptr)
                    press(*key);
            };
            const auto snapshot = [&](juce::Component& panel, const juce::String& file)
            {
                if (snapshots.isNotEmpty())
                {
                    dir.createDirectory();
                    writePng(panel.createComponentSnapshot(panel.getLocalBounds(), true, 2.0f), dir.getChildFile(file + ".png"));
                }
            };

            beginTest("the_page_s_keys_make_the_job_and_are_kept");
            {
                auto panel = exportPage();
                expectEquals(ui::optionsPageNameForTesting(*panel), juce::String("Export"));
                for (auto* child : panel->getChildren())
                    if (child->isVisible())
                        expect(panel->getLocalBounds().contains(child->getBounds()), child->getComponentID() + ": outside the panel");
                auto job = ui::exportJobForTesting(*panel);
                expect(! job.chain && job.pattern == 2 && job.variationB, "the pattern shown, 03B");
                expectEquals(job.times, 1);
                expect(job.end == ExportEnd::cut && ! job.dry);
                expectEquals(job.sampleRate, p->captureRate(), "the app's rate");
                for (const auto* id : { "exportWhat1", "exportTimes3", "exportEnd2", "exportFiles1", "exportRate1" })
                    choose(*panel, id);
                job = ui::exportJobForTesting(*panel);
                expect(job.chain, "the chain");
                expectEquals(job.times, 8);
                expect(job.end == ExportEnd::loop, "a seamless loop");
                expect(job.dry, "the dry voice too");
                expectEquals(job.sampleRate, 96000.0);
                panel.reset();
                // a new page (the window opened again) shows the choices kept
                panel = exportPage();
                job = ui::exportJobForTesting(*panel);
                expect(job.chain && job.times == 8 && job.end == ExportEnd::loop && job.dry && job.sampleRate == 96000.0, "kept for next time");
                for (const auto* id : { "exportWhat0", "exportTimes1", "exportEnd1", "exportFiles0", "exportRate0" })
                    choose(*panel, id);
                job = ui::exportJobForTesting(*panel);
                expect(! job.chain && job.times == 2 && job.end == ExportEnd::ring && ! job.dry && job.sampleRate == 48000.0);
            }

            beginTest("export_writes_a_24_bit_wav_and_the_dry_file");
            {
                auto panel = exportPage();
                for (const auto* id : { "exportTimes0", "exportEnd0", "exportFiles1" })
                    choose(*panel, id);
                const auto job = ui::exportJobForTesting(*panel);
                const auto mixFile = folder.getChildFile("Page.wav");
                ui::exportToForTesting(*panel, mixFile);
                expect(ui::exportBusyForTesting(*panel), "busy while it renders");
                auto* cancelKey = childWithId(*panel, "exportCancel");
                auto* exportKey = childWithId(*panel, "exportGo");
                expect(cancelKey != nullptr && cancelKey->isVisible() && exportKey != nullptr && ! exportKey->isVisible(), "Cancel in Export's place");
                expect(ui::finishExportForTesting(*panel, 60000), "done within 60 s");
                expect(exportKey != nullptr && exportKey->isVisible(), "Export back");
                expect(ui::exportStatusForTesting(*panel).startsWith("Written: Page.wav and Page - dry.wav"), ui::exportStatusForTesting(*panel));
                for (const auto& f : { mixFile, dryFile(mixFile) })
                {
                    expect(f.existsAsFile(), f.getFileName());
                    juce::WavAudioFormat wav;
                    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(f.createInputStream().release(), true));
                    expect(reader != nullptr, f.getFileName());
                    if (reader != nullptr)
                    {
                        expectEquals(static_cast<int>(reader->bitsPerSample), 24);
                        expectEquals(reader->sampleRate, 48000.0);
                        expectEquals(static_cast<juce::int64>(reader->lengthInSamples), static_cast<juce::int64>(loopSamples(job)), "cut where the time through ends");
                    }
                }
                snapshot(*panel, "options-export-written");
            }

            beginTest("cancel_or_closing_leaves_no_file");
            {
                const auto mixFile = folder.getChildFile("Long.wav");
                {
                    // a long one: sixteen times, a seamless loop, the dry voice too
                    auto panel = exportPage();
                    for (const auto* id : { "exportTimes4", "exportEnd2", "exportFiles1" })
                        choose(*panel, id);
                    ui::exportToForTesting(*panel, mixFile);
                    juce::Thread::sleep(300);
                    auto* cancelKey = childWithId(*panel, "exportCancel");
                    expect(cancelKey != nullptr);
                    if (cancelKey != nullptr)
                        press(*cancelKey);
                    expect(ui::finishExportForTesting(*panel, 30000), "stopped soon after");
                    expectEquals(ui::exportStatusForTesting(*panel), juce::String("Cancelled; nothing written."));
                    expect(! mixFile.exists() && ! dryFile(mixFile).exists(), "no file");
                }
                {
                    // the window closed while it renders
                    auto panel = exportPage();
                    ui::exportToForTesting(*panel, mixFile);
                    juce::Thread::sleep(300);
                    expect(ui::exportBusyForTesting(*panel));
                    panel.reset();
                    expect(! mixFile.exists() && ! dryFile(mixFile).exists(), "no file");
                }
            }

            if (snapshots.isNotEmpty())
            {
                beginTest("snapshots_of_the_export_page_in_graphite_and_silver");
                const int themeBefore = ui::currentTheme();
                for (const int t : { 0, 1 })
                {
                    e->chooseThemeForTesting(t);
                    const auto themeName = ui::themeNames()[t].toLowerCase();
                    auto panel = exportPage();
                    snapshot(*panel, "options-export-" + themeName);
                    ui::exportToForTesting(*panel, folder.getChildFile("Snap.wav"));
                    juce::Thread::sleep(4000);
                    snapshot(*panel, "options-export-busy-" + themeName);
                    panel.reset(); // cancels it
                }
                e->chooseThemeForTesting(themeBefore);
                expect(! folder.getChildFile("Snap.wav").exists());
            }

            (void) folder.deleteRecursively();
            e.reset();
            p.reset();
        }
    };

    static OptionsExportTests optionsExportTests;
}
