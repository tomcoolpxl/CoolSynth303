#pragma once

#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Widgets.h"

namespace cs303
{
    class Editor;
}

namespace cs303::ui
{
    /** The pattern keys 01 to 16 in a bay, A and B in another, the chain (each pattern with its repeats) in a
        display with + and Loop, and the pattern's length (CoolSynth's D350, layout B). */
    class PatternRow final : public juce::Component
    {
    public:
        explicit PatternRow(Editor&);
        static constexpr int rowY = 3; // the keys' top: the bays reach 3 px above and below them
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        [[nodiscard]] juce::Rectangle<int> slotBounds(int i) const;
        [[nodiscard]] juce::Rectangle<int> variationBounds(bool b) const;
        [[nodiscard]] juce::Rectangle<int> chainLabelBounds() const;
        [[nodiscard]] juce::Rectangle<int> chainBounds(int i) const;
        [[nodiscard]] juce::Rectangle<int> addBounds() const;
        [[nodiscard]] juce::Rectangle<int> loopBounds() const;

    private:
        Editor& editor;
        static constexpr int maxShown = 4;
        std::unique_ptr<ValueBox> length;
        /** How many of the chain's entries are drawn: all if they fit, else one fewer and a count of the rest. */
        [[nodiscard]] int shownChain() const;
        /** Right-click on a pattern key: copy, paste, clear, A to B, rename. */
        void patternMenu(int index);
    };

    /** The tools that change the whole line, down the inspector's Line page: a new line (Generate, Mutate and what
        it may change), the key and Snap to key, Move, Pitch. */
    class ToolRow final : public juce::Component
    {
    public:
        explicit ToolRow(Editor&);
        void paint(juce::Graphics&) override;
        void refresh();

    private:
        Editor& editor;
        std::vector<std::unique_ptr<juce::Component>> parts;
        std::vector<std::pair<juce::String, juce::Rectangle<int>>> labels; // a group's name in its place
        Key& button(const juce::String& text, juce::Rectangle<int> place, bool hot = false);
        ValueBox& box(juce::Rectangle<int> place);
    };

    /** The step grid: the step lights, each step's note, the pitch lane with the notes down its edge, the rows for
        octave, accent, slide and gate, and one knob's recorded moves. */
    class StepGrid final : public juce::Component
    {
    public:
        explicit StepGrid(Editor&);
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

        static constexpr int labelWidth = 108;
        enum class Row { lights, names, pitch, octave, accent, slide, gate, motion, none };
        [[nodiscard]] juce::Rectangle<float> cell(Row row, int step) const;
        [[nodiscard]] juce::Rectangle<float> rowBounds(Row row) const;
        [[nodiscard]] int columnAt(float x) const;
        [[nodiscard]] Row rowAt(float y) const;

    private:
        Editor& editor;
        Row dragRow = Row::none;
        int dragColumn = -1;
        void setPitchFromY(int step, float y);
        void setMotionAt(juce::Point<float> p);
        void stepMenu(int step);
    };

    /** The picked step's locks: a knob each for cutoff, env mod, decay and accent, and Clear. */
    class LocksStrip final : public juce::Component
    {
    public:
        explicit LocksStrip(Editor&);
        void paint(juce::Graphics&) override;
        void refresh();

    private:
        Editor& editor;
        std::vector<std::unique_ptr<juce::Component>> parts;
    };

    /** The column beside the grid: two tabs, Locks (the picked step's) and Line (the tools for the whole line). */
    class InspectorColumn final : public juce::Component
    {
    public:
        explicit InspectorColumn(Editor&);
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void refresh();
        void showLine(bool line);
        [[nodiscard]] bool lineShown() const noexcept { return line; }
        [[nodiscard]] static juce::Rectangle<int> tabBounds(bool lineTab) { return lineTab ? juce::Rectangle<int> { 77, 7, 66, 24 } : juce::Rectangle<int> { 7, 7, 68, 24 }; }

    private:
        Editor& editor;
        std::unique_ptr<LocksStrip> locks;
        std::unique_ptr<ToolRow> tools;
        bool line = true;
    };

    class SequencerSection final : public juce::Component
    {
    public:
        explicit SequencerSection(Editor&);
        void paint(juce::Graphics&) override;
        void refresh();
        void repaintPlaying();
        [[nodiscard]] StepGrid& grid() noexcept { return *stepGrid; }

    private:
        Editor& editor;
        std::unique_ptr<PatternRow> patterns;
        std::unique_ptr<StepGrid> stepGrid;
        std::unique_ptr<InspectorColumn> column;
        std::vector<std::unique_ptr<juce::Component>> headKeys; // Undo, Redo and Drag MIDI in the head band
    };
}
