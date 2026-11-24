// The options' own window (D349): its pages build in the standalone's form (with a device manager) and a plugin's
// (without), every part inside the panel, and the Accuracy keys set the processor's accuracy. With
// COOLSYNTH303_SNAPSHOTS=<folder> it also writes each page and the top bar with its Options key. A class of its own:
// MSVC's Debug frames overflow with many processors and editors in one runTest.

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Editor.h"
#include "Processor.h"
#include "ui/OptionsWindow.h"

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
            }

            devices.reset();
            e.reset();
            p.reset();
        }
    };

    static OptionsTests optionsTests;
}
