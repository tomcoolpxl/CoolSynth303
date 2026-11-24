#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

/*
    The panel's controls, drawn in the charcoal look: a knob, a key with its lamp, a fader, a value in a box. Each is
    wired with functions (get, set, onClick) as CoolSynth 808's widgets are, so the panel holds no copy of the
    project's values.
*/
namespace cs303::ui
{
    /** A black knob with an orange pointer, ticks round it, its range's 0 and 10 below and its name under them.
        Dragged up and down (Shift for fine), wheeled, double-clicked back to its default. */
    class Knob final : public juce::Component
    {
    public:
        Knob(juce::String name, float diameter);

        std::function<float()> get;             // 0..1
        std::function<void(float)> set;
        std::function<void()> gestureStart, gestureEnd;
        std::function<float()> follow;          // when set and >= 0: the value the pattern moves it to, drawn
        std::function<bool()> dimmed;           // drawn as not holding its own value (a lock not set)
        std::function<juce::String()> valueText; // when set: a value shown beside the name (the delay's division)
        float defaultValue = 0.5f;
        bool showRange = true;
        bool accentRim = false;                 // an orange body: the accent's lock knob

        [[nodiscard]] float diameter() const noexcept { return size; }
        /** The size this knob takes: the dial with its ticks, the range and the name. */
        [[nodiscard]] juce::Rectangle<int> preferredBounds(int x, int y) const;

        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        void mouseDoubleClick(const juce::MouseEvent&) override;
        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    private:
        juce::String name;
        float size;
        float dragStartValue = 0.0f;
        void change(float v);
    };

    /** A light grey key that turns orange when on, its lamp above it (lit when on) unless it is plain. */
    class Key final : public juce::Component
    {
    public:
        explicit Key(juce::String text, bool lamp = true);

        std::function<bool()> isOn;
        std::function<void()> onClick;
        std::function<void()> onRightClick;
        std::function<juce::String()> textOf;
        std::function<bool()> enabled;
        bool lamp;
        bool hot = false;                       // the panel's one orange action (Generate)
        int glyph = 0;                          // 1 a saw, 2 a square drawn in place of the name

        /** Lamp keys sit 8 px below their place, the lamp in that room above them, as the mockup lays them out. */
        static constexpr int lampRoom = 8;

        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;

    private:
        juce::String text;
    };

    /** A vertical fader: a dark cap with an orange line on a scale. */
    class Fader final : public juce::Component
    {
    public:
        explicit Fader(juce::String name);
        std::function<float()> get;
        std::function<void(float)> set;
        std::function<void()> gestureStart, gestureEnd;
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;

    private:
        juce::String name;
        float dragStartValue = 0.0f;
    };

    /** A value in a dark box; clicked, right-clicked or wheeled to change it. */
    class ValueBox final : public juce::Component
    {
    public:
        ValueBox() = default;
        std::function<juce::String()> text;
        std::function<void()> onClick, onRightClick;
        std::function<void(int)> onWheel; // +1 up, -1 down
        juce::Colour ink { 0xffffb35c }; // amber, as the displays
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    };

    /** Plain text drawn in a place (a group's name, a hint). */
    class Caption final : public juce::Component
    {
    public:
        Caption() { setInterceptsMouseClicks(false, false); }
        std::function<juce::String()> text;
        juce::Font font { juce::FontOptions(11.0f) };
        juce::Colour ink { 0xff9fa1a5 };
        juce::Justification justification { juce::Justification::centredLeft };
        bool multiline = false;
        void paint(juce::Graphics&) override;
    };
}
