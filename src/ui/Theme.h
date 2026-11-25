#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Text.h"

#include <functional>

/*
    The machined look the user chose (CoolSynth's D348, the mockups' 2 Redesign): plates on a brushed panel, each
    section's name in a head band with the signal path printed through them, knurled knobs with an aluminium cap and a
    lit value arc, keys with a lens that lights, amber displays, the sequencer under a dark screen. Its colours are a
    palette per theme (D352, docs/design/mockups/coolsynth-303-themes.src.html in CoolSynth's repository): Graphite,
    Silver after the TB-303's face, the 808's Steel and Olive. The names below refer into the palette in use, so a
    theme chosen changes them all; the plates' printing (label, dim...) and the screens' (screenLabel...) are apart,
    because on Silver the plates are light and the screens stay dark.
*/
namespace cs303::ui
{
    struct Palette
    {
        // the frame: the top bar, the title strip, the dialogs (dark glass in every theme)
        juce::Colour desk, face, dialogTop, dialogBottom, bar, barTop, barBottom, barDim, stencil, stencilRight, titleInk;
        // the panel's ground and its plates
        juce::Colour faceTop, faceBottom, plateTop, plateBottom, plateEdge;
        float brush, sheen, plateLit, plateShadow;
        // printing on the plates, and inside the dark screens
        juce::Colour caption, label, dim, hint, rowName, labelHover, valueInk;
        juce::Colour screenCaption, screenLabel, screenDim, screenHint;
        // the keys
        juce::Colour capTop, capMid, capBottom, capEdge, capInk, capInkOn, capEngrave;
        float capLit, engraveDy;
        // the signal
        juce::Colour orange, orangeInk, hotTop, hotBottom, hotDeep, amber, accentNote, light, lensLit, lensCore, lensGlow, tieTop, tieBottom;
        // the screens, displays and the grid's keys
        juce::Colour screenTop, screenBottom, screenEdge, displayTop, displayBottom, well, wellLine, outline;
        juce::Colour cell, cellOff, litTop, litBottom, halfTop, halfBottom, slotHasTop, slotHasBottom, slotEmptyTop, slotEmptyBottom, key, keyInk;
        juce::Colour lampOff, lampOn, ink;
    };

    /** The palette in use; themes() lists them, Graphite first (the default). */
    [[nodiscard]] Palette& palette();
    [[nodiscard]] juce::StringArray themeNames();
    [[nodiscard]] int currentTheme();
    /** Makes theme i the palette in use and drops the pictures kept in the old one's colours; the caller repaints. */
    void applyTheme(int i);

    namespace colour
    {
        inline const juce::Colour& desk = palette().desk;
        inline const juce::Colour& face = palette().face;          // a dialog's or a list's ground
        inline const juce::Colour& faceTop = palette().faceTop;
        inline const juce::Colour& faceBottom = palette().faceBottom;
        inline const juce::Colour& bar = palette().bar;            // the window's title bar
        inline const juce::Colour& stencil = palette().stencil;    // the strip with the name
        inline const juce::Colour& plateTop = palette().plateTop;
        inline const juce::Colour& plateBottom = palette().plateBottom;
        inline const juce::Colour& outline = palette().outline;
        inline const juce::Colour& caption = palette().caption;    // on the plates: a section's name
        inline const juce::Colour& label = palette().label;        // a knob's name
        inline const juce::Colour& dim = palette().dim;            // a group's name
        inline const juce::Colour& hint = palette().hint;
        inline const juce::Colour& rowName = palette().rowName;
        inline const juce::Colour& screenCaption = palette().screenCaption; // inside the dark screens and dialogs
        inline const juce::Colour& screenLabel = palette().screenLabel;
        inline const juce::Colour& screenDim = palette().screenDim;
        inline const juce::Colour& screenHint = palette().screenHint;
        inline const juce::Colour& key = palette().key;            // a lit cell, a chosen page
        inline const juce::Colour& keyInk = palette().keyInk;
        inline const juce::Colour& capInk = palette().capInk;      // a key's name
        inline const juce::Colour& orange = palette().orange;      // on: the signal colour
        inline const juce::Colour& orangeInk = palette().orangeInk;
        inline const juce::Colour& amber = palette().amber;        // a display's figures
        inline const juce::Colour& lampOff = palette().lampOff;
        inline const juce::Colour& lampOn = palette().lampOn;      // the load's warning
        inline const juce::Colour& well = palette().well;          // a display
        inline const juce::Colour& wellLine = palette().wellLine;
        inline const juce::Colour& cell = palette().cell;          // a grid's key, off
        inline const juce::Colour& cellOff = palette().cellOff;
        inline const juce::Colour& light = palette().light;        // the note playing
        inline const juce::Colour& accentNote = palette().accentNote;
        inline const juce::Colour& ink = palette().ink;            // text on dark glass
        inline const juce::Colour& valueInk = palette().valueInk;  // a value printed on a plate
    }

    enum class Face { label, labelBold, head, mono, monoBold, body, stencil };
    [[nodiscard]] juce::Font fontOf(Face face, float height, float tracking = 0.0f);
    [[nodiscard]] inline juce::Font labelFont(float h = 10.5f) { return fontOf(Face::labelBold, h, 0.08f); }
    [[nodiscard]] inline juce::Font monoFont(float h = 11.0f) { return fontOf(Face::mono, h); }
    [[nodiscard]] inline juce::Font bodyFont(float h = 11.0f) { return fontOf(Face::body, h, 0.02f); }

    /** A still picture drawn once by paint into an image at the scale it is shown at, kept by its name and size, and
        drawn from there after (CoolSynth's D306). */
    void drawKept(juce::Graphics& g, const juce::String& name, juce::Rectangle<float> area, const std::function<void(juce::Graphics&, juce::Rectangle<float>)>& paint);
    /** The panel's ground: brushed graphite, a sheen from above (kept). */
    void drawGround(juce::Graphics& g, juce::Rectangle<float> area);
    /** A section: a raised plate from 8 px above the area's top, its name in a head band; a number (1 on) printed
        before the name in orange. Returns where the name ends, for the signal path. */
    float drawSection(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& name, juce::Colour background, int number = 0);
    /** A key: a dark cap; with a lens (lit when on) if it has one; hot is the panel's one orange action. */
    void drawKey(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, bool on, bool lens, bool enabled = true, bool hot = false,
                 bool hover = false, bool down = false);
    /** The light over a painted control under the mouse (D353, as the 808's keys light under it). */
    void drawHover(juce::Graphics& g, juce::Rectangle<float> r, float corner = 3.0f);
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
