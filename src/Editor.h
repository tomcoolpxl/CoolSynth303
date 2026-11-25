#pragma once

#include <memory>
#include <optional>

#include <juce_audio_processors/juce_audio_processors.h>

#include "Pattern.h"

namespace cs303
{
    class Processor;
    namespace ui
    {
        class SynthPanel;
        class ModsPanel;
        class KeyboardSection;
        class SequencerSection;
        class TopBar;
        class LidView;
        class LibraryShelf;
        class OptionsWindow;
        struct OptionsHooks;
    }
    struct LibraryLine;
    struct LibrarySound;

    /*
        CoolSynth 303's window: the layout the user picked, 2.3 (CoolSynth's D302): the top bar, the synth's sections
        in the signal's order, the keyboard over three octaves, the sequencer with its step grid and the picked step's
        locks. Drawn at 1280 by 800 and scaled to the window, as CoolSynth 808's.

        What the panel shows but the project does not keep lives here: the step picked, the pattern edited, the
        keyboard's mode, which knob's moves the motion lane shows, what Mutate may change.
    */
    class Editor final : public juce::AudioProcessorEditor, private juce::Timer, private juce::ComponentListener
    {
    public:
        static constexpr int baseWidth = 1280, baseHeight = 800;

        explicit Editor(Processor&);
        ~Editor() override;

        void paint(juce::Graphics&) override;
        void resized() override;
        bool keyPressed(const juce::KeyPress&) override;
        bool keyStateChanged(bool isKeyDown) override;

        Processor& processor;

        // ---------------------------------------------------------------- what the panel holds
        int pick = 0;                 // the step the keys write at and the locks show
        int editPattern = 0;
        bool editB = false;
        bool transposeMode = false;
        bool modsOpen = false;        // the Mods panel over the synth's sections
        void setModsOpen(bool open);
        /** Under the Lid (D310): the circuit over the whole window, at the screen's own pixels. */
        void setLidOpen(bool open);
        [[nodiscard]] ui::LidView& lidView() noexcept { return *lid; }
        /** The library (D338): the shelf over the board; closing it ends an audition and gives yours back. */
        void openLibrary();
        void closeLibrary();
        [[nodiscard]] ui::LibraryShelf* libraryShelf() noexcept { return library.get(); }
        /** A line into the pattern you edit, with its tempo; a sound onto the panel. One undo step each. */
        void loadLine(const LibraryLine& line);
        void loadSound(const LibrarySound& sound);
        /** An audition (null ends it): what plays is the library's, without an undo step, until stopped or loaded. */
        void auditionLine(const LibraryLine* line);
        void auditionSound(const LibrarySound* sound);
        [[nodiscard]] bool isAuditioning() const noexcept { return auditionSaved != nullptr; }
        /** The options (D349): a window of their own above this one, opened and closed by the top bar's Options key. */
        void toggleOptions();
        [[nodiscard]] bool optionsOpen() const noexcept;
        /** What the options act on: the processor's accuracy, this window's size, the theme, the WAV export's job. */
        [[nodiscard]] ui::OptionsHooks optionsHooks();
        /** The window's size in percent of the base size, and a size chosen in the options (kept in the app's settings). */
        [[nodiscard]] int windowSizePercent() const;
        void chooseWindowSize(int percent);
        /** A theme chosen in the options (D352), kept in the app's settings: the whole panel drawn in it at once. */
        void chooseTheme(int index);
        /** As chooseTheme, without keeping it in the app's settings (the tests' pictures of every theme). */
        void chooseThemeForTesting(int index) { applyChosenTheme(index); }
        Knob motionKnob = Knob::cutoff;
        MutateWhat mutateWhat {};
        std::optional<Pattern> clipboard; // a pattern copied from a pattern key's menu
        std::uint32_t nextSeed = 0x303u;

        [[nodiscard]] const Pattern& edited() const;
        /** Changes the edited pattern (one undo step) and redraws. */
        void editPatternWith(const std::function<void(Pattern&)>& change, bool undoable = true);
        /** Picks a pattern to edit and to hear (at once while stopped, at its end while playing). */
        void choosePattern(int index, bool variationB);
        /** A key of the piano (0..36): writes it at the picked step and moves on, or transposes. */
        void playKey(int key);
        void setGateAtPick(Gate gate);
        void togglePick(bool accent);
        void movePick(int by);
        void togglePlay();
        /** Redraws what an edit may have changed: the top bar, the keyboard, the sequencer; the synth's knobs and the
            Mods panel only when the panel's values changed, or with everything (CoolSynth's D306: a whole redraw is what
            made a press show late on the 808). */
        void refreshAll(bool everything = false);
        /** Lays every part out at the base size, for tests and snapshots without a window. */
        void layOut();

        [[nodiscard]] juce::Component& board() noexcept { return *boardHolder; }
        [[nodiscard]] ui::TopBar& topBarForTesting() noexcept { return *top; }
        /** What the hint line shows with the mouse at a place on the board (D353). */
        [[nodiscard]] juce::String hintAt(juce::Point<int> onBoard);

    private:
        void timerCallback() override;
        void componentBroughtToFront(juce::Component&) override;

        std::unique_ptr<juce::Component> boardHolder;
        std::unique_ptr<juce::Component> hintLine;          // the panel's foot: what the control under the mouse does (D353)
        std::unique_ptr<juce::MouseListener> hintWatcher;
        void applyChosenTheme(int index);
        std::unique_ptr<ui::OptionsWindow> optionsWindow;
        juce::Component* watchedWindow = nullptr; // the window this editor is in, while the options keep above it
        std::unique_ptr<ui::TopBar> top;
        std::unique_ptr<ui::SynthPanel> synth;
        std::unique_ptr<ui::ModsPanel> mods;
        std::unique_ptr<ui::KeyboardSection> keys;
        std::unique_ptr<ui::SequencerSection> sequencer;
        std::unique_ptr<ui::LidView> lid;
        std::unique_ptr<ui::LibraryShelf> library;
        std::unique_ptr<Project> auditionSaved; // the project as it was before an audition
        bool auditionStartedPlaying = false;
        void stopAudition();
        void beginAudition();
        std::uint32_t lastEdits = 0;
        int lastStep = -2, lastPattern = -1, lastQueued = -2, lastSounding = -2, lastSyncShown = -1;
        bool lastPlaying = false;
        Panel shownPanel {};     // the panel's values as the knobs last drew them
        bool panelShown = false;
        int heldComputerKey = -1;
    };
}
