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
    /** The pattern keys 01 to 16, A and B, the chain (each pattern with its repeats) and Loop. */
    class PatternRow final : public juce::Component
    {
    public:
        explicit PatternRow(Editor&);
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
        static constexpr int maxShown = 8;
    };

    /** The tools, each group named: a new line (Generate, Mutate and what it may change), the key, Move, Pitch,
        Length, Undo and Redo. */
    class ToolRow final : public juce::Component
    {
    public:
        explicit ToolRow(Editor&);
        void paint(juce::Graphics&) override;
        void refresh();

    private:
        Editor& editor;
        std::vector<std::unique_ptr<juce::Component>> parts;
        std::vector<std::pair<juce::String, int>> groupLabels; // a group's name at its x
        std::vector<int> separators;
        int x = 0;
        Key& button(const juce::String& text, int w, bool on = false);
        ValueBox& box(int w);
        void group(const juce::String& name);
        void endGroup();
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
        std::unique_ptr<ToolRow> tools;
        std::unique_ptr<StepGrid> stepGrid;
        std::unique_ptr<LocksStrip> locks;
    };
}
