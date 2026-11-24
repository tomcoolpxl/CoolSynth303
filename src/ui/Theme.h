#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/*
    The charcoal look the user chose (CoolSynth's D296, D300 to D302; TAL-Vocoder 3's way of drawing): a flat dark
    panel, sections outlined in thin light lines with their names in capitals, black knobs with an orange pointer,
    light grey keys that turn orange with a red lamp when on. Colours and type as the mockup gives them
    (docs/design/mockups/coolsynth-303-layouts.html in CoolSynth's repository).
*/
namespace cs303::ui
{
    namespace colour
    {
        inline const juce::Colour desk { 0xff0b0a09 };      // around the panel when the window's shape differs
        inline const juce::Colour face { 0xff26272a };      // the panel
        inline const juce::Colour bar { 0xff1c1d20 };       // the top bar
        inline const juce::Colour stencil { 0xff1f2023 };   // the strip with the name
        inline const juce::Colour outline { 0xff6b6d72 };   // the sections' lines
        inline const juce::Colour caption { 0xffe9e9e6 };   // a section's name
        inline const juce::Colour label { 0xffd7d8da };     // a knob's name
        inline const juce::Colour dim { 0xff8d8f94 };       // a group's name, arrows
        inline const juce::Colour hint { 0xff9fa1a5 };
        inline const juce::Colour key { 0xffc9cacc };       // a key, off
        inline const juce::Colour keyInk { 0xff1b1c1e };
        inline const juce::Colour orange { 0xffe8742a };    // on
        inline const juce::Colour orangeInk { 0xff1a120c };
        inline const juce::Colour lampOff { 0xff4a1c17 };
        inline const juce::Colour lampOn { 0xffff3b28 };
        inline const juce::Colour well { 0xff111214 };      // a value's box
        inline const juce::Colour wellLine { 0xff3a3b3f };
        inline const juce::Colour cell { 0xff3a3b3f };      // a grid's key, off
        inline const juce::Colour cellOff { 0xff2d2e31 };
        inline const juce::Colour light { 0xffffd2a6 };     // the note playing
        inline const juce::Colour accentNote { 0xffffab6b };
        inline const juce::Colour ink { 0xfff1ebde };
    }

    enum class Face { label, labelBold, head, mono, monoBold, body, stencil };
    [[nodiscard]] juce::Font fontOf(Face face, float height, float tracking = 0.0f);
    [[nodiscard]] inline juce::Font labelFont(float h = 10.5f) { return fontOf(Face::labelBold, h, 0.08f); }
    [[nodiscard]] inline juce::Font monoFont(float h = 11.0f) { return fontOf(Face::mono, h); }
    [[nodiscard]] inline juce::Font bodyFont(float h = 11.0f) { return fontOf(Face::body, h, 0.02f); }

    /** A section: a rounded outline with its name set into the top line. */
    void drawSection(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& name, juce::Colour background);
    /** A key of the charcoal look: light grey or orange, its lamp above (or none). */
    void drawKey(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, bool on, bool lamp, bool enabled = true);
    void drawLamp(juce::Graphics& g, juce::Point<float> centre, bool on);
    /** A small dark box holding a value. */
    void drawWell(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, juce::Colour ink);
}
