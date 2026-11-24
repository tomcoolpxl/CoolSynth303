#pragma once

#include <memory>

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
    }

    /*
        CoolSynth 303's window: the layout the user picked, 2.3 (CoolSynth's D302): the top bar, the synth's sections
        in the signal's order, the keyboard over three octaves, the sequencer with its step grid and the picked step's
        locks. Drawn at 1280 by 800 and scaled to the window, as CoolSynth 808's.

        What the panel shows but the project does not keep lives here: the step picked, the pattern edited, the
        keyboard's mode, which knob's moves the motion lane shows, what Mutate may change.
    */
    class Editor final : public juce::AudioProcessorEditor, private juce::Timer
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
        Knob motionKnob = Knob::cutoff;
        MutateWhat mutateWhat {};
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
        void refreshAll();
        /** Lays every part out at the base size, for tests and snapshots without a window. */
        void layOut();

        [[nodiscard]] juce::Component& board() noexcept { return *boardHolder; }

    private:
        void timerCallback() override;

        std::unique_ptr<juce::Component> boardHolder;
        std::unique_ptr<ui::TopBar> top;
        std::unique_ptr<ui::SynthPanel> synth;
        std::unique_ptr<ui::ModsPanel> mods;
        std::unique_ptr<ui::KeyboardSection> keys;
        std::unique_ptr<ui::SequencerSection> sequencer;
        std::uint32_t lastEdits = 0;
        int lastStep = -2, lastPattern = -1, lastQueued = -2, lastSounding = -2;
        bool lastPlaying = false;
        int heldComputerKey = -1;
    };
}
