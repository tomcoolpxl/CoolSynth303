// The library (D338): the built-in lines and sounds read in whole, a sound set onto the panel, your own kept,
// renamed and read back (in a temporary folder), and the shelf's audition, load and keep as a player uses them;
// with COOLSYNTH303_SNAPSHOTS=<folder> it also writes a picture of the shelf.

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Editor.h"
#include "Library.h"
#include "Processor.h"
#include "ui/LibraryShelf.h"
#include "ui/Theme.h"

namespace
{
    using namespace cs303;

    /** A few blocks of audio, so what the transport was asked to do has happened. */
    void run(Processor& p)
    {
        juce::AudioBuffer<float> buffer(2, 480);
        for (int b = 0; b < 4; ++b)
        {
            juce::MidiBuffer midi;
            p.processBlock(buffer, midi);
        }
    }

    class LibraryTests final : public juce::UnitTest
    {
    public:
        LibraryTests() : juce::UnitTest("Library", "CoolSynth303") {}

        void runTest() override
        {
            const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("CoolSynth303LibraryTest");
            folder.deleteRecursively();
            setUserLibraryFolderForTesting(folder);

            beginTest("the_built_in_lines_and_sounds_are_all_there");
            {
                const auto lib = builtInLibrary();
                expectEquals(static_cast<int>(lib.lines.size()), 30);
                expectEquals(static_cast<int>(lib.sounds.size()), 12);
                juce::StringArray styles, soundNames;
                for (const auto& s : lib.sounds)
                    soundNames.add(s.name);
                for (const auto& l : lib.lines)
                {
                    styles.addIfNotAlreadyThere(l.style);
                    expect(l.pattern.length >= 1 && l.pattern.length <= 16, l.name);
                    expect(soundNames.contains(l.sound), l.name + ": its sound is on the shelf");
                    int notes = 0;
                    for (int i = 0; i < l.pattern.length; ++i)
                        notes += l.pattern.step[static_cast<size_t>(i)].gate == Gate::note ? 1 : 0;
                    expect(notes >= 3, l.name + ": a line, not silence");
                }
                expectEquals(styles.size(), 10);
            }

            beginTest("a_sound_sets_what_it_names_and_leaves_the_rest");
            {
                Panel panel;
                panel.volume = 0.3f;
                panel.tuning = 0.61f;
                const auto lib = builtInLibrary();
                const auto& screamer = *std::find_if(lib.sounds.begin(), lib.sounds.end(), [](const LibrarySound& s) { return s.name == "Devil Screamer"; });
                const auto after = applySound(panel, screamer.values);
                expect(after.devilFish);
                expectWithinAbsoluteError(static_cast<double>(after.overdrive), 0.7, 1.0e-6);
                expectWithinAbsoluteError(static_cast<double>(after.volume), 0.3, 1.0e-6);
                expectWithinAbsoluteError(static_cast<double>(after.tuning), 0.61, 1.0e-6);
                expect(! soundValuesOf(after).hasProperty("volume"), "a sound kept from the panel leaves the volume out");
            }

            beginTest("yours_are_kept_renamed_and_read_back");
            {
                const auto lib = builtInLibrary();
                const auto f = saveUserLine(lib.lines.front());
                expect(f.existsAsFile());
                const auto again = saveUserLine(lib.lines.front());
                expect(again != f, "a taken name gets a number");
                const auto renamed = renameUserEntry(f, "Mine");
                expect(renamed.existsAsFile() && ! f.existsAsFile());
                const auto mine = userLibrary();
                expectEquals(static_cast<int>(mine.lines.size()), 2);
                expect(std::any_of(mine.lines.begin(), mine.lines.end(), [](const LibraryLine& l) { return l.name == "Mine" && ! l.builtIn; }));
                const auto sameSteps = [&](const LibraryLine& l) {
                    return l.pattern.step == lib.lines.front().pattern.step && l.pattern.length == lib.lines.front().pattern.length;
                };
                expect(sameSteps(mine.lines.front()) && sameSteps(mine.lines.back()), "the steps come back as kept");
                saveUserSound(lib.sounds.front());
                expectEquals(static_cast<int>(userLibrary().sounds.size()), 1);
            }

            auto p = std::make_unique<Processor>();
            p->setPlayConfigDetails(0, 2, 48000.0, 480);
            p->prepareToPlay(48000.0, 480);
            auto e = std::make_unique<Editor>(*p);
            e->layOut();

            beginTest("the_shelf_auditions_and_loads_as_one_undo_step");
            {
                const auto before = p->project();
                const bool undoBefore = p->canUndo();
                e->openLibrary();
                auto* shelf = e->libraryShelf();
                expect(shelf != nullptr);
                if (shelf == nullptr)
                    return;
                shelf->showShelf("lines:Acid techno");
                expectEquals(shelf->shownCount(), 3);
                shelf->selectRow(0);
                const auto* line = shelf->chosenLine();
                expect(line != nullptr && line->name == "Twelve Cycle", line != nullptr ? "chose " + line->name : juce::String("no line"));
                if (line == nullptr)
                    return;
                logMessage("pattern 1A's name before: " + juce::String(p->project().pattern(0, false).name) + ", the line's pattern name: "
                           + juce::String(line->pattern.name));
                shelf->toggleAudition();
                expect(e->isAuditioning());
                expect(p->project().pattern(0, false) == line->pattern, "the line plays in place of yours");
                expect(p->canUndo() == undoBefore, "an audition takes no undo step");
                shelf->toggleAudition();
                expect(! e->isAuditioning());
                expect(p->project() == before, "stopped, yours comes back as it was");

                shelf->load();
                expect(p->project().pattern(0, false) == line->pattern);
                expectWithinAbsoluteError(p->project().bpm, line->bpm, 1.0e-9);
                expect(p->canUndo());
                p->undo();
                expect(p->project() == before, "one undo step takes the load back");

                // Walking the list while listening (D354): the audition moves along in one step, the transport keeps
                // playing, the shelf's key stays on Stop, and Stop gives yours back.
                shelf->selectRow(0);
                shelf->toggleAudition();
                run(*p);
                expect(e->isAuditioning() && shelf->isAuditioning() && p->isPlaying());
                shelf->selectRow(1);
                run(*p);
                expect(shelf->isAuditioning() && p->isPlaying(), "the next line plays at once, no stop between");
                expect(p->project().pattern(0, false) == shelf->chosenLine()->pattern, "the second line in place of yours");
                shelf->showShelf("sounds:Stock");
                expect(! shelf->isAuditioning(), "another shelf stops it");
                shelf->selectRow(0);
                shelf->toggleAudition();
                run(*p);
                shelf->selectRow(2);
                run(*p);
                expect(shelf->isAuditioning() && p->isPlaying(), "from sound to sound, still sounding");
                shelf->toggleAudition();
                run(*p);
                expect(! p->isPlaying(), "Stop stops the transport the audition started");
                expect(! shelf->isAuditioning() && ! e->isAuditioning());
                expect(p->project() == before, "stopped, yours comes back as it was");

                shelf->showShelf("sounds:Devil Fish");
                shelf->selectRow(0);
                shelf->toggleAudition();
                expect(p->project().panel.devilFish, "a sound auditioned plays");
                shelf->load();
                expect(! e->isAuditioning() && p->project().panel.devilFish, "loaded, it stays");
                p->undo();
                expect(p->project() == before);

                // A click on a line keeps the library open (D358): the list's clicks reach the shelf too (its hover
                // light listens to them), and were taken for a click on the veil. A click on the veil still closes.
                {
                    juce::ListBox* listBox = nullptr;
                    std::function<void(juce::Component&)> find = [&](juce::Component& c)
                    {
                        for (auto* child : c.getChildren())
                        {
                            if (auto* l = dynamic_cast<juce::ListBox*>(child); l != nullptr && listBox == nullptr)
                                listBox = l;
                            find(*child);
                        }
                    };
                    find(*shelf);
                    // off screen the list makes no row components: its own click stands for a row's (both reach the
                    // shelf from inside the list)
                    juce::Component* row = listBox;
                    expect(row != nullptr, "the list");
                    const auto click = [](juce::Component& on, juce::Point<float> at)
                    {
                        const auto now = juce::Time::getCurrentTime();
                        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), at, {}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                                &on, &on, now, at, now, 1, false);
                    };
                    if (row != nullptr)
                    {
                        const auto kept = shelf->onClose;
                        bool closed = false;
                        shelf->onClose = [&closed] { closed = true; };
                        // a row's own place: near its top left, outside the shelf's box as the shelf counts
                        shelf->mouseUp(click(*row, { 4.0f, 4.0f }));
                        expect(! closed, "a click on a line leaves the library open");
                        shelf->mouseUp(click(*shelf, { 4.0f, 4.0f }));
                        expect(closed, "a click on the veil closes it");
                        shelf->onClose = kept;
                    }
                }

                shelf->showShelf("lines:Trance");
                shelf->selectRow(1);
                shelf->keepCopy();
                expect(shelf->shownCount() >= 1, "kept, it is on your shelf");

                if (const auto dir = juce::SystemStats::getEnvironmentVariable("COOLSYNTH303_SNAPSHOTS", {}); dir.isNotEmpty())
                {
                    shelf->showShelf("lines:Long slides");
                    shelf->selectRow(0);
                    // in every theme (D352), then back to the one in use
                    const int themeBefore = ui::currentTheme();
                    const auto names = ui::themeNames();
                    for (int t = -1; t < names.size(); ++t)
                    {
                        if (t >= 0)
                            e->chooseThemeForTesting(t);
                        const auto image = e->board().createComponentSnapshot(e->board().getLocalBounds(), true, t < 0 ? 1.0f : 2.0f);
                        auto file = juce::File(dir).getChildFile(t < 0 ? juce::String("library.png") : "library-" + names[t].toLowerCase() + ".png");
                        file.deleteFile();
                        juce::FileOutputStream out(file);
                        juce::PNGImageFormat().writeImageToStream(image, out);
                    }
                    e->chooseThemeForTesting(themeBefore);
                }
                e->closeLibrary();
                expect(e->libraryShelf() == nullptr);
            }

            e = nullptr;
            p = nullptr;
            setUserLibraryFolderForTesting({});
            folder.deleteRecursively();
        }
    };

    static LibraryTests libraryTests;
}
