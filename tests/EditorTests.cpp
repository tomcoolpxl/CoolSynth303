// The panel: it builds and lays out, its keys write and transpose; with COOLSYNTH303_SNAPSHOTS=<folder> it also
// writes pictures of itself, stopped and playing, for a look without opening the app.

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Editor.h"
#include "Processor.h"

namespace
{
    using namespace cs303;

    void playFor(Processor& p, double seconds)
    {
        juce::AudioBuffer<float> buffer(2, 480);
        for (int b = 0; b < static_cast<int>(seconds * 100.0); ++b)
        {
            juce::MidiBuffer midi;
            p.processBlock(buffer, midi);
        }
    }

    void snapshot(Editor& e, const juce::File& folder, const juce::String& name)
    {
        e.layOut();
        e.refreshAll();
        const auto image = e.board().createComponentSnapshot(e.board().getLocalBounds(), true, 2.0f);
        auto file = folder.getChildFile(name + ".png");
        file.deleteFile();
        juce::FileOutputStream out(file);
        juce::PNGImageFormat().writeImageToStream(image, out);
    }

    class EditorTests final : public juce::UnitTest
    {
    public:
        EditorTests() : juce::UnitTest("Editor", "CoolSynth303") {}

        void runTest() override
        {
            auto p = std::make_unique<Processor>();
            p->setPlayConfigDetails(0, 2, 48000.0, 480);
            p->prepareToPlay(48000.0, 480);
            auto e = std::make_unique<Editor>(*p);

            beginTest("the_panel_builds_and_lays_out_at_several_sizes");
            {
                for (auto [w, h] : { std::pair { 1280, 800 }, std::pair { 640, 400 }, std::pair { 1920, 1080 } })
                {
                    e->setSize(w, h);
                    expect(e->board().getWidth() == Editor::baseWidth);
                }
                e->setSize(Editor::baseWidth, Editor::baseHeight);
            }

            beginTest("a_key_in_write_sets_the_picked_step_and_moves_on");
            {
                e->pick = 3;
                e->playKey(30);
                expectEquals(p->project().pattern(0, false).step[3].key(), 30);
                expect(p->project().pattern(0, false).step[3].gate == Gate::note);
                expectEquals(e->pick, 4);
                e->setGateAtPick(Gate::tie);
                expect(p->project().pattern(0, false).step[4].gate == Gate::tie);
                expectEquals(e->pick, 5);
                p->undo();
                p->undo();
                expectEquals(p->project().pattern(0, false).step[3].key(), defaultProject().pattern(0, false).step[3].key());
            }

            beginTest("a_key_in_transpose_shifts_from_the_middle_c");
            {
                e->transposeMode = true;
                e->playKey(19);
                expectEquals(p->transposition(), 7);
                e->playKey(12);
                expectEquals(p->transposition(), 0);
                e->transposeMode = false;
            }

            beginTest("choosing_a_pattern_edits_it");
            {
                e->choosePattern(2, true);
                expectEquals(e->editPattern, 2);
                expect(e->editB);
                e->choosePattern(0, false);
            }

            const auto folder = juce::SystemStats::getEnvironmentVariable("COOLSYNTH303_SNAPSHOTS", {});
            if (folder.isNotEmpty())
            {
                beginTest("snapshots");
                const juce::File dir(folder);
                dir.createDirectory();
                e->pick = 4;
                snapshot(*e, dir, "panel-stopped");
                p->setPlaying(true);
                playFor(*p, 0.6);
                snapshot(*e, dir, "panel-playing");
                e->transposeMode = true;
                snapshot(*e, dir, "panel-transpose");
                e->transposeMode = false;
                p->setPlaying(false);
                playFor(*p, 0.05);
                expect(true);
            }
            e.reset();
        }
    };

    static EditorTests editorTests;
}
