#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/*
    The machined look the user chose (CoolSynth's D348, the mockups' 2 Redesign): graphite plates on a brushed panel,
    each section's name in a head band with the signal path printed through them, knurled knobs with an aluminium
    cap and a lit value arc, dark keys with a lens that lights, amber displays, the sequencer under a dark screen.
    Colours and sizes as the mockup gives them (docs/design/mockups/coolsynth-303-redesign.src.html in CoolSynth's
    repository).
*/
namespace cs303::ui
{
    namespace colour
    {
        inline const juce::Colour desk { 0xff0b0a09 };      // around the panel when the window's shape differs
        inline const juce::Colour face { 0xff1c1d20 };      // the panel (a window's or a list's ground)
        inline const juce::Colour faceTop { 0xff1f2023 }, faceBottom { 0xff18191b };
        inline const juce::Colour bar { 0xff121315 };       // the top bar
        inline const juce::Colour stencil { 0xff141517 };   // the strip with the name
        inline const juce::Colour plateTop { 0xff26272b }, plateBottom { 0xff212226 };
        inline const juce::Colour outline { 0xff3a3c41 };   // a line where a plate would be too much
        inline const juce::Colour caption { 0xffecebe6 };   // a section's name
        inline const juce::Colour label { 0xffe2e1dc };     // a knob's name
        inline const juce::Colour dim { 0xff7d7f84 };       // a group's name
        inline const juce::Colour hint { 0xff8f9196 };
        inline const juce::Colour rowName { 0xffd9d7d0 };
        inline const juce::Colour key { 0xff4f5156 };       // a lit cell, a chosen page
        inline const juce::Colour keyInk { 0xfff2efe8 };
        inline const juce::Colour capInk { 0xffe7e5df };    // a key's name
        inline const juce::Colour orange { 0xfff0782a };    // on
        inline const juce::Colour orangeInk { 0xff1d0f05 };
        inline const juce::Colour amber { 0xffffb35c };     // a display's figures
        inline const juce::Colour lampOff { 0xff5a2a20 };
        inline const juce::Colour lampOn { 0xffff3b28 };    // the load's warning
        inline const juce::Colour well { 0xff0d0e0f };      // a display
        inline const juce::Colour wellLine { 0xff2a2b2f };
        inline const juce::Colour cell { 0xff2a2b2f };      // a grid's key, off
        inline const juce::Colour cellOff { 0xff222326 };
        inline const juce::Colour light { 0xfffff1e0 };     // the note playing
        inline const juce::Colour accentNote { 0xffffc489 };
        inline const juce::Colour ink { 0xfff1ebde };
    }

    enum class Face { label, labelBold, head, mono, monoBold, body, stencil };
    [[nodiscard]] juce::Font fontOf(Face face, float height, float tracking = 0.0f);
    [[nodiscard]] inline juce::Font labelFont(float h = 10.5f) { return fontOf(Face::labelBold, h, 0.08f); }
    [[nodiscard]] inline juce::Font monoFont(float h = 11.0f) { return fontOf(Face::mono, h); }
    [[nodiscard]] inline juce::Font bodyFont(float h = 11.0f) { return fontOf(Face::body, h, 0.02f); }

    /** The panel's ground: brushed graphite, a sheen from above. */
    void drawGround(juce::Graphics& g, juce::Rectangle<float> area);
    /** A section: a raised plate from 8 px above the area's top, its name in a head band; a number (1 on) printed
        before the name in orange. Returns where the name ends, for the signal path. */
    float drawSection(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& name, juce::Colour background, int number = 0);
    /** A key: a dark cap; with a lens (lit when on) if it has one; hot is the panel's one orange action. */
    void drawKey(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, bool on, bool lens, bool enabled = true, bool hot = false);
    /** A lens in a key, or a lamp on its own: dark red glass, amber and glowing when lit. */
    void drawLens(juce::Graphics& g, juce::Point<float> centre, bool on, float diameter = 6.0f);
    inline void drawLamp(juce::Graphics& g, juce::Point<float> centre, bool on) { drawLens(g, centre, on); }
    /** A display: a dark window behind glass holding a value. */
    void drawWell(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, juce::Colour ink);
    void drawDisplay(juce::Graphics& g, juce::Rectangle<float> area);
    /** A recessed dark screen (the sequencer's grid, the locks, the keyboard's bed). */
    void drawScreen(juce::Graphics& g, juce::Rectangle<float> area, float corner = 6.0f);

    /** A key in the sequencer's grid. */
    enum class Cell { off, lit, half, hot };
    void drawCell(juce::Graphics& g, juce::Rectangle<float> r, Cell look, float alpha = 1.0f);

    /** A soft glow under a shape of the signal colour. */
    void glowRounded(juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::Colour c, float spread = 3.0f, float alpha = 0.35f);
}
