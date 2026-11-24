#include "ui/Theme.h"

#include "AppData.h"

namespace cs303::ui
{
    namespace
    {
        juce::Typeface::Ptr typefaceOf(Face face)
        {
            static const juce::Typeface::Ptr label = juce::Typeface::createSystemTypefaceFor(AppData::ArchivoSemiCondensedSemiBold_ttf, AppData::ArchivoSemiCondensedSemiBold_ttfSize);
            static const juce::Typeface::Ptr labelBold = juce::Typeface::createSystemTypefaceFor(AppData::ArchivoSemiCondensedBold_ttf, AppData::ArchivoSemiCondensedBold_ttfSize);
            static const juce::Typeface::Ptr head = juce::Typeface::createSystemTypefaceFor(AppData::ArchivoExtraBold_ttf, AppData::ArchivoExtraBold_ttfSize);
            static const juce::Typeface::Ptr mono = juce::Typeface::createSystemTypefaceFor(AppData::IBMPlexMonoMedium_ttf, AppData::IBMPlexMonoMedium_ttfSize);
            static const juce::Typeface::Ptr monoBold = juce::Typeface::createSystemTypefaceFor(AppData::IBMPlexMonoSemiBold_ttf, AppData::IBMPlexMonoSemiBold_ttfSize);
            static const juce::Typeface::Ptr body = juce::Typeface::createSystemTypefaceFor(AppData::ArchivoMedium_ttf, AppData::ArchivoMedium_ttfSize);
            static const juce::Typeface::Ptr stencil = juce::Typeface::createSystemTypefaceFor(AppData::ArchivoCondensedSemiBold_ttf, AppData::ArchivoCondensedSemiBold_ttfSize);
            switch (face)
            {
                case Face::label: return label;
                case Face::labelBold: return labelBold;
                case Face::head: return head;
                case Face::mono: return mono;
                case Face::monoBold: return monoBold;
                case Face::body: return body;
                case Face::stencil: return stencil;
            }
            return label;
        }
    }

    juce::Font fontOf(Face face, float height, float tracking)
    {
        return juce::Font(juce::FontOptions().withTypeface(typefaceOf(face)).withHeight(height)).withExtraKerningFactor(tracking);
    }

    void drawSection(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& name, juce::Colour background)
    {
        g.setColour(colour::outline);
        g.drawRoundedRectangle(area.reduced(0.5f), 9.0f, 1.0f);
        if (name.isEmpty())
            return;
        const auto font = labelFont(11.0f).withExtraKerningFactor(0.12f);
        const auto text = name.toUpperCase();
        const float w = juce::GlyphArrangement::getStringWidth(font, text) + 12.0f;
        const juce::Rectangle<float> gap(area.getX() + 10.0f, area.getY() - 7.0f, w, 14.0f);
        g.setColour(background);
        g.fillRect(gap);
        g.setColour(colour::caption);
        g.setFont(font);
        g.drawText(text, gap, juce::Justification::centred, false);
    }

    void drawLamp(juce::Graphics& g, juce::Point<float> centre, bool on)
    {
        if (on)
        {
            g.setColour(colour::lampOn.withAlpha(0.35f));
            g.fillEllipse(juce::Rectangle<float>(11.0f, 11.0f).withCentre(centre));
        }
        g.setColour(on ? colour::lampOn : colour::lampOff);
        g.fillEllipse(juce::Rectangle<float>(5.0f, 5.0f).withCentre(centre));
    }

    void drawKey(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, bool on, bool lamp, bool enabled)
    {
        if (lamp)
            drawLamp(g, { area.getCentreX(), area.getY() - 5.5f }, on);
        g.setColour(juce::Colours::black);
        g.fillRoundedRectangle(area.translated(0.0f, 1.0f), 2.0f);
        g.setColour((on ? colour::orange : colour::key).withMultipliedAlpha(enabled ? 1.0f : 0.45f));
        g.fillRoundedRectangle(area, 2.0f);
        g.setColour(on ? colour::orangeInk : colour::keyInk);
        g.setFont(labelFont(10.5f));
        g.drawText(text.toUpperCase(), area, juce::Justification::centred, false);
    }

    void drawWell(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, juce::Colour ink)
    {
        g.setColour(colour::well);
        g.fillRoundedRectangle(area, 3.0f);
        g.setColour(colour::wellLine);
        g.drawRoundedRectangle(area.reduced(0.5f), 3.0f, 1.0f);
        g.setColour(ink);
        g.setFont(monoFont(11.5f));
        g.drawText(text, area, juce::Justification::centred, false);
    }
}
