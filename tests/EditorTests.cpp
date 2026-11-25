// The panel: it builds and lays out, its keys write and transpose; with COOLSYNTH303_SNAPSHOTS=<folder> it also
// writes pictures of itself, stopped and playing, for a look without opening the app.

#include <algorithm>
#include <functional>
#include <typeinfo>
#include <memory>
#include <vector>
#include <tuple>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Editor.h"
#include "Processor.h"
#include "ui/Keyboard.h"
#include "ui/LidView.h"
#include "ui/Panel.h"
#include "ui/SequencerView.h"

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

    /** The first component of a kind in the panel. */
    template <class T>
    T* find(juce::Component& root)
    {
        if (auto* t = dynamic_cast<T*>(&root))
            return t;
        for (auto* child : root.getChildren())
            if (auto* t = find<T>(*child))
                return t;
        return nullptr;
    }

    /** A click as a player's: at a place on the board, on whatever is drawn on top there. Returns what it hit. */
    juce::Component* click(juce::Component& board, juce::Point<float> at, bool right = false)
    {
        auto* target = board.getComponentAt(at.roundToInt());
        if (target == nullptr)
            return nullptr;
        const auto local = target->getLocalPoint(&board, at);
        const auto now = juce::Time::getCurrentTime();
        const juce::MouseEvent e(juce::Desktop::getInstance().getMainMouseSource(), local,
                                 right ? juce::ModifierKeys(juce::ModifierKeys::rightButtonModifier | juce::ModifierKeys::popupMenuClickModifier)
                                       : juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),
                                 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, target, target, now, local, now, 1, false);
        target->mouseDown(e);
        target->mouseUp(e.withNewPosition(local));
        return target;
    }

    template <class T>
    juce::Point<float> onBoard(juce::Component& board, T& part, juce::Point<float> local)
    {
        return board.getLocalPoint(&part, local);
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

            beginTest("clicks_land_where_the_panel_draws_them");
            {
                auto& board = e->board();
                auto* grid = find<ui::StepGrid>(board);
                auto* piano = find<ui::Piano>(board);
                auto* row = find<ui::PatternRow>(board);
                expect(grid != nullptr && piano != nullptr && row != nullptr);
                if (grid != nullptr && piano != nullptr && row != nullptr)
                {
                    using R = ui::StepGrid::Row;
                    const bool accentBefore = p->project().pattern(0, false).step[3].accent;
                    expect(click(board, onBoard(board, *grid, grid->cell(R::accent, 3).getCentre())) == grid);
                    expect(p->project().pattern(0, false).step[3].accent != accentBefore, "the accent key of step 4");
                    expectEquals(e->pick, 3);

                    click(board, onBoard(board, *grid, grid->cell(R::gate, 7).getCentre()));
                    expect(p->project().pattern(0, false).step[7].gate != defaultProject().pattern(0, false).step[7].gate, "the gate key of step 8");

                    // a drag on the pitch lane sets the note from its height: the top row is the C above
                    const auto lane = grid->cell(R::pitch, 9);
                    click(board, onBoard(board, *grid, juce::Point<float>(lane.getCentreX(), lane.getY() + 2.0f)));
                    expectEquals(p->project().pattern(0, false).step[9].note, 12);
                    click(board, onBoard(board, *grid, juce::Point<float>(lane.getCentreX(), lane.getBottom() - 2.0f)));
                    expectEquals(p->project().pattern(0, false).step[9].note, 0);

                    // a black key of the piano, over the white ones: the middle C sharp, written at the pick
                    e->pick = 2;
                    expect(click(board, onBoard(board, *piano, piano->keyBounds(13).getCentre())) == piano);
                    expectEquals(p->project().pattern(0, false).step[2].key(), 13);
                    expectEquals(e->pick, 3);
                    // the top C, the last white key
                    e->pick = 2;
                    click(board, onBoard(board, *piano, piano->keyBounds(36).getCentre().translated(0.0f, 30.0f)));
                    expectEquals(p->project().pattern(0, false).step[2].key(), 36);

                    // a pattern key: pattern 05 to edit
                    click(board, onBoard(board, *row, row->slotBounds(4).toFloat().getCentre()));
                    expectEquals(e->editPattern, 4);
                    click(board, onBoard(board, *row, row->variationBounds(true).toFloat().getCentre()));
                    expect(e->editB);
                    e->choosePattern(0, false);
                    // + adds the pattern edited to the chain
                    const auto chainBefore = p->project().chain.size();
                    click(board, onBoard(board, *row, row->addBounds().toFloat().getCentre()));
                    expectEquals(static_cast<int>(p->project().chain.size()), static_cast<int>(chainBefore) + 1);
                    p->undo();
                }
                playFor(*p, 0.05);
            }

            beginTest("what_a_paint_costs");
            {
                // The board painted at the window's usual scale (1.75), whole and in the areas a press or the beat
                // repaints: a step's key, a knob. The software renderer, not the window's Direct2D, so for comparing.
                auto& board = e->board();
                auto* grid = find<ui::StepGrid>(board);
                auto* knob = find<ui::Knob>(board);
                expect(grid != nullptr && knob != nullptr);
                if (grid != nullptr && knob != nullptr)
                {
                    const auto cost = [&board](juce::Rectangle<int> area) {
                        std::vector<double> ms;
                        for (int i = 0; i < 9; ++i)
                        {
                            juce::Image image(juce::Image::ARGB, 2240, 1400, true);
                            juce::Graphics g(image);
                            g.addTransform(juce::AffineTransform::scale(1.75f));
                            g.reduceClipRegion(area);
                            const double t0 = juce::Time::getMillisecondCounterHiRes();
                            board.paintEntireComponent(g, false);
                            ms.push_back(juce::Time::getMillisecondCounterHiRes() - t0);
                        }
                        std::sort(ms.begin(), ms.end());
                        return ms[ms.size() / 2];
                    };
                    using R = ui::StepGrid::Row;
                    const auto step = board.getLocalArea(grid, grid->cell(R::accent, 3).toNearestInt());
                    const auto knobArea = board.getLocalArea(knob, knob->getLocalBounds());
                    const auto gridArea = board.getLocalArea(grid, grid->getLocalBounds());
                    logMessage("paint, ms (median of 9): whole " + juce::String(cost(board.getLocalBounds()), 1) + ", the grid "
                               + juce::String(cost(gridArea), 1) + ", a step's key " + juce::String(cost(step), 2) + ", a knob "
                               + juce::String(cost(knobArea), 2));
                }
            }

            beginTest("every_control_has_a_hint");
            {
                // the hint line (D353): every key, knob, fader and value box says what it does
                juce::StringArray missing;
                std::function<void(juce::Component&)> walk = [&](juce::Component& c) {
                    const bool control = dynamic_cast<ui::Key*>(&c) != nullptr || dynamic_cast<ui::Knob*>(&c) != nullptr
                                      || dynamic_cast<ui::Fader*>(&c) != nullptr || dynamic_cast<ui::ValueBox*>(&c) != nullptr;
                    if (control && c.getHelpText().isEmpty())
                        missing.add(c.getParentComponent() != nullptr ? juce::String(typeid(*c.getParentComponent()).name()) + " at " + c.getBounds().toString() : juce::String("?"));
                    for (auto* child : c.getChildren())
                        walk(*child);
                };
                walk(e->board());
                expect(missing.isEmpty(), "no hint: " + missing.joinIntoString("; "));
                auto* grid = find<ui::StepGrid>(e->board());
                expect(grid != nullptr);
                if (grid != nullptr)
                {
                    using R = ui::StepGrid::Row;
                    expect(grid->hintAt(grid->cell(R::accent, 3).getCentre().toInt()).startsWith("Step 4's accent"));
                    expect(grid->hintAt(grid->cell(R::pitch, 0).getCentre().toInt()).startsWith("Step 1's note"));
                    // as the line finds them, from a place on the board: a painted place, a key, a knob
                    auto& board = e->board();
                    auto* row = find<ui::PatternRow>(board);
                    auto* knob = find<ui::Knob>(board);
                    if (row != nullptr && knob != nullptr)
                    {
                        expect(e->hintAt(board.getLocalPoint(row, row->slotBounds(4).getCentre())).startsWith("Pattern 05"));
                        expect(e->hintAt(board.getLocalPoint(knob, knob->getLocalBounds().getCentre())).contains("double-click resets"));
                        expect(e->hintAt(board.getLocalPoint(grid, grid->cell(R::gate, 2).getCentre().toInt())).startsWith("Step 3's gate"));
                    }
                }
            }

            beginTest("the_inspector_column_shows_the_locks_or_the_line_by_its_tabs");
            {
                auto& board = e->board();
                auto* column = find<ui::InspectorColumn>(board);
                auto* tools = find<ui::ToolRow>(board);
                auto* locks = find<ui::LocksStrip>(board);
                expect(column != nullptr && tools != nullptr && locks != nullptr);
                if (column != nullptr && tools != nullptr && locks != nullptr)
                {
                    expect(column->lineShown() && tools->isVisible() && ! locks->isVisible(), "the Line page first");
                    expect(click(board, onBoard(board, *column, ui::InspectorColumn::tabBounds(false).toFloat().getCentre())) == column);
                    expect(! column->lineShown() && locks->isVisible() && ! tools->isVisible(), "Locks after its tab");
                    click(board, onBoard(board, *column, ui::InspectorColumn::tabBounds(true).toFloat().getCentre()));
                    expect(column->lineShown() && tools->isVisible(), "Line again");
                }
            }

            beginTest("the_devil_fish_switch_in_the_mods_panel_turns_it_on_and_off");
            {
                auto& board = e->board();
                e->setModsOpen(true);
                auto* mods = find<ui::ModsPanel>(board);
                expect(mods != nullptr && mods->isVisible());
                if (mods != nullptr)
                {
                    // the On key: its place in the Devil Fish's section, below its lamp
                    const juce::Point<float> on(static_cast<float>(ui::ModsPanel::fishX + 14 + 29), 10.0f + 22.0f + 8.0f + 12.0f);
                    expect(! p->project().panel.devilFish);
                    click(board, onBoard(board, *mods, on));
                    expect(p->project().panel.devilFish, "on");
                    playFor(*p, 0.05);
                    expect(p->isDevilFishPlaying(), "and playing");
                    click(board, onBoard(board, *mods, on));
                    expect(! p->project().panel.devilFish, "off again");
                }
                e->setModsOpen(false);
            }

            beginTest("under_the_lid_draws_the_whole_voice_from_the_engine");
            {
                auto& lid = e->lidView();
                e->setLidOpen(true);
                expect(lid.isVisible() && lid.isOpen());
                expectEquals(lid.drawingCheck(), juce::String("all matching"), "the sheet as the mockup checked it");
                expectGreaterThan(lid.partCount(), 190);
                expectGreaterThan(lid.flowCount(), 800);
                const auto unserved = lid.unservedSeries();
                expect(unserved.isEmpty(), "every series the drawing needs comes from the engine: missing " + unserved.joinIntoString(", "));
                p->setPlaying(true);
                playFor(*p, 0.5);
                lid.tickNow(1.0);
                lid.tickNow(1.03);
                expectGreaterThan(lid.flowsLit(), 100, "the current lights the wires while a pattern plays");
                lid.setReplay(true);
                lid.tickNow(1.06);
                expectGreaterThan(lid.flowsLit(), 50, "the held seconds replay");
                lid.setReplay(false);
                p->setPlaying(false);
                playFor(*p, 0.05);
                e->setLidOpen(false);
                expect(! lid.isVisible() && ! lid.isOpen());
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
                e->setModsOpen(true);
                snapshot(*e, dir, "panel-mods");
                p->edit([](Project& pr) { pr.panel.devilFish = true; pr.panel.overdrive = 0.45f; pr.panel.sweepSpeed = 2; }, false);
                snapshot(*e, dir, "panel-devil-fish");
                p->edit([](Project& pr) { pr.panel = Panel {}; }, false);
                e->setModsOpen(false);
                // the lid while the first line plays, at the window's base size and at 1920 by 1080
                for (auto [w, h, shot] : { std::tuple { 1280, 800, "lid" }, std::tuple { 1920, 1080, "lid-1080" } })
                {
                    e->setSize(w, h);
                    e->setLidOpen(true);
                    p->setPlaying(true);
                    playFor(*p, 0.4);
                    e->lidView().tickNow(10.0);
                    e->lidView().tickNow(10.033);
                    e->lidView().select("xvcf.r62");
                    const auto image = e->lidView().createComponentSnapshot(e->lidView().getLocalBounds(), true, 1.0f);
                    auto file = dir.getChildFile(juce::String(shot) + ".png");
                    file.deleteFile();
                    juce::FileOutputStream out(file);
                    juce::PNGImageFormat().writeImageToStream(image, out);
                    e->setLidOpen(false);
                }
                e->setSize(Editor::baseWidth, Editor::baseHeight);
                p->setPlaying(false);
                playFor(*p, 0.05);
                expect(true);
            }
            e.reset();
        }
    };

    static EditorTests editorTests;
}
