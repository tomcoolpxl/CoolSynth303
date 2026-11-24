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
    /** The top bar: the pattern's name between two arrows, New, Open, Save; the transport, the tempo, Tap, Rec. */
    class TopBar final : public juce::Component
    {
    public:
        explicit TopBar(Editor&);
        void paint(juce::Graphics&) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        void refresh();

    private:
        Editor& editor;
        std::vector<std::unique_ptr<Key>> keys;
        Key *prev = nullptr, *next = nullptr, *play = nullptr, *rec = nullptr, *mods = nullptr, *lid = nullptr;
        juce::Rectangle<int> nameBox, tempoBox;
        double dragStartBpm = 0.0;
        std::vector<double> taps;
        std::unique_ptr<juce::FileChooser> chooser;
        Key& add(const juce::String& text);
        void openProject();
        void saveProject();
        void tap();
    };

    /** The Mods panel, over the synth's sections while open (CoolSynth's D296, D309): the trimmers inside the case, as
        fitted to a real unit, and Stock to set them back; the Devil Fish, switched on or off, with its knobs, its
        three switches, its red Accent button and Stock. */
    class ModsPanel final : public juce::Component
    {
    public:
        explicit ModsPanel(Editor&);
        void paint(juce::Graphics&) override;
        void refresh();

        static constexpr int fishX = 324, fishRight = 1204; // the Devil Fish's section

    private:
        Editor& editor;
        std::vector<std::unique_ptr<juce::Component>> parts;
        void buildDevilFish();
    };

    /** The synth's sections in the signal's order with arrows between: oscillator, filter, envelope, amplifier,
        drive and delay, as the 303's own knobs and switch and the two effects after it. */
    class SynthPanel final : public juce::Component
    {
    public:
        explicit SynthPanel(Editor&);
        void paint(juce::Graphics&) override;
        void refresh();

    private:
        struct SectionPlace
        {
            const char* name;
            int x, w;
        };
        Editor& editor;
        std::vector<std::unique_ptr<juce::Component>> parts;
        std::vector<SectionPlace> sections;
        Knob& knob(const juce::String& name, float size, int sectionX, int x, int y);
        Key& key(const juce::String& text, int sectionX, int x, int y, int w);
    };
}
