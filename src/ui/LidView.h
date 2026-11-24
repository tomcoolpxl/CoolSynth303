#pragma once

#include <functional>
#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

namespace cs303
{
    class Processor;
}

namespace cs303::ui
{
    /*
        Under the Lid (CoolSynth's D310): the TB-303's whole voice as one schematic sheet, every part, the current on
        its wires as moving dots and each wire's voltage as a tint, live from the engine's own solve.

        The sheet is CoolSynth's mockup (docs/design/mockups/coolsynth-303-lid.html), laid out there, checked against
        voice.cir and exported as data (resources/lid/tb303-sheet.json, tools/lid/export.ps1). Each wire's current is
        the sum of its parts' currents beyond it (the mockup worked it out by Kirchhoff); the values come from the
        engine's Tb303Lid, read by the audio thread 4000 times a second while the lid is open (Processor::lidFrame).

        Live shows the circuit as it plays, the dots keeping a short afterglow of what happened between two screen
        frames. Slow replay holds the last two seconds and walks them fifty times slower on a time bar. Wheel to zoom,
        drag to pan, Fit; click a part to read it.
    */
    class LidView final : public juce::Component, private juce::Timer
    {
    public:
        explicit LidView(Processor&);
        ~LidView() override;

        std::function<void()> onClose;

        /** Opens or closes it: the audio thread reads the circuit only while it is open. */
        void setOpen(bool open);
        [[nodiscard]] bool isOpen() const noexcept { return open; }

        void paint(juce::Graphics&) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        bool keyPressed(const juce::KeyPress&) override;

        // ---------------------------------------------------------------- for the tests
        [[nodiscard]] int partCount() const noexcept;
        [[nodiscard]] int flowCount() const noexcept;
        /** The drawing's own check against the netlist, as the mockup wrote it ("all matching"). */
        [[nodiscard]] juce::String drawingCheck() const;
        /** Series the drawing needs that the engine does not serve. */
        [[nodiscard]] juce::StringArray unservedSeries() const;
        /** Takes the frames the audio thread has written so far (a timer tick, without the timer). */
        void tickNow(double seconds);
        void setReplay(bool replay);
        /** The wires carrying a visible current now. */
        [[nodiscard]] int flowsLit() const noexcept;
        /** Selects a part by its netlist name (xvcf.r62), as a click on it does. */
        void select(const juce::String& element);

    private:
        void timerCallback() override;
        struct Impl;
        std::unique_ptr<Impl> impl;
        Processor& processor;
        bool open = false;
    };
}
