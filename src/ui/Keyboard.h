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
    /** The piano over the 303's three octaves (D302): C an octave down to the C two octaves up. A key held while
        stopped sounds; in Write it sets the picked step's note and octave and moves on, in Transpose it shifts the
        pattern (the middle C plays it as written). */
    class Piano final : public juce::Component
    {
    public:
        explicit Piano(Editor&);
        static constexpr int whiteWidth = 29, keys = 37, whites = 22;
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        [[nodiscard]] int keyAt(juce::Point<float>) const;
        [[nodiscard]] juce::Rectangle<float> keyBounds(int key) const;

    private:
        Editor& editor;
        int held = -1;
    };

    /** The keyboard section: Accent and Slide, the piano with its octaves named, Back, Step, Tie and Rest, Write or
        Transpose, and what a key does now. */
    class KeyboardSection final : public juce::Component
    {
    public:
        explicit KeyboardSection(Editor&);
        void paint(juce::Graphics&) override;
        void refresh();
        void repaintPiano() { piano->repaint(); }

    private:
        Editor& editor;
        std::unique_ptr<Piano> piano;
        std::vector<std::unique_ptr<juce::Component>> parts;
        const juce::Rectangle<int> modeBay { 952, 51, 176, 30 }; // Write and Transpose, one switch
        Key& key(const juce::String& text, int x, int y, int w);
    };
}
