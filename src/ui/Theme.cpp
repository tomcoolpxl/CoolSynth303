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

    namespace
    {
        juce::ColourGradient vertical(juce::Rectangle<float> r, std::initializer_list<std::pair<float, juce::Colour>> stops)
        {
            juce::ColourGradient grad(stops.begin()->second, r.getX(), r.getY(), (stops.end() - 1)->second, r.getX(), r.getBottom(), false);
            for (const auto& [at, c] : stops)
                if (at > 0.0f && at < 1.0f)
                    grad.addColour(at, c);
            return grad;
        }
    }

    void glowRounded(juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::Colour c, float spread, float alpha)
    {
        // a few widening, fading copies: enough of a halo at the panel's sizes without a blur
        for (int i = 3; i >= 1; --i)
        {
            const float e = spread * static_cast<float>(i) / 3.0f;
            g.setColour(c.withAlpha(alpha * (0.18f + 0.27f * static_cast<float>(3 - i))));
            g.fillRoundedRectangle(r.expanded(e), corner + e);
        }
    }

    void drawGround(juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setGradientFill(vertical(area, { { 0.0f, colour::faceTop }, { 1.0f, colour::faceBottom } }));
        g.fillRect(area);
        // the brushing: a hairline every other pixel, barely there
        g.setColour(juce::Colours::white.withAlpha(0.014f));
        for (float x = area.getX(); x < area.getRight(); x += 2.0f)
            g.fillRect(x, area.getY(), 1.0f, area.getHeight());
        // the sheen from above
        juce::ColourGradient sheen(juce::Colours::white.withAlpha(0.07f), area.getX() + area.getWidth() * 0.55f, area.getY() - 40.0f,
                                   juce::Colours::white.withAlpha(0.0f), area.getX() + area.getWidth() * 0.55f, area.getY() + 260.0f, true);
        g.setGradientFill(sheen);
        g.fillRect(area);
    }

    float drawSection(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& name, juce::Colour, int number)
    {
        const auto plate = area.withTop(area.getY() - 8.0f);
        constexpr float corner = 7.0f;
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRoundedRectangle(plate.translated(0.0f, 3.0f).expanded(1.0f), corner + 1.0f);
        g.setColour(juce::Colour(0xff0c0c0d));
        g.fillRoundedRectangle(plate.expanded(1.0f), corner + 1.0f);
        g.setGradientFill(vertical(plate, { { 0.0f, colour::plateTop }, { 1.0f, colour::plateBottom } }));
        g.fillRoundedRectangle(plate, corner);
        // the head band
        const auto band = plate.withHeight(22.0f);
        {
            juce::Graphics::ScopedSaveState s(g);
            juce::Path clip;
            clip.addRoundedRectangle(band.getX(), band.getY(), band.getWidth(), band.getHeight(), corner, corner, true, true, false, false);
            g.reduceClipRegion(clip);
            g.setGradientFill(vertical(band, { { 0.0f, juce::Colours::white.withAlpha(0.045f) }, { 1.0f, juce::Colours::white.withAlpha(0.015f) } }));
            g.fillRect(band);
        }
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillRect(band.getX(), band.getBottom() - 1.0f, band.getWidth(), 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.04f));
        g.fillRect(band.getX(), band.getBottom(), band.getWidth(), 1.0f);
        // the plate's lit top edge
        g.setColour(juce::Colours::white.withAlpha(0.07f));
        g.fillRect(plate.getX() + corner, plate.getY(), plate.getWidth() - 2.0f * corner, 1.0f);

        float x = plate.getX() + 12.0f;
        if (number > 0)
        {
            const auto font = fontOf(Face::monoBold, 10.5f);
            const auto t = juce::String(number).paddedLeft('0', 2);
            g.setColour(colour::orange);
            g.setFont(font);
            g.drawText(t, juce::Rectangle<float>(x, band.getY(), 30.0f, band.getHeight()), juce::Justification::centredLeft, false);
            x += juce::GlyphArrangement::getStringWidth(font, t) + 7.0f;
        }
        if (name.isEmpty())
            return x;
        const auto font = fontOf(Face::head, 10.0f, 0.15f);
        const auto text = name.toUpperCase();
        g.setColour(colour::caption);
        g.setFont(font);
        g.drawText(text, juce::Rectangle<float>(x, band.getY(), plate.getRight() - x - 8.0f, band.getHeight()), juce::Justification::centredLeft, false);
        return x + juce::GlyphArrangement::getStringWidth(font, text);
    }

    void drawLens(juce::Graphics& g, juce::Point<float> centre, bool on, float d)
    {
        const auto r = juce::Rectangle<float>(d, d).withCentre(centre);
        if (on)
        {
            for (int i = 3; i >= 1; --i)
            {
                g.setColour(juce::Colour(0xffff8c32).withAlpha(0.12f + 0.1f * static_cast<float>(3 - i)));
                g.fillEllipse(r.expanded(static_cast<float>(i) * 1.6f));
            }
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.fillEllipse(r.expanded(1.0f));
            juce::ColourGradient grad(juce::Colour(0xfffff4e6), r.getX() + d * 0.38f, r.getY() + d * 0.32f, colour::orange, r.getRight(), r.getBottom(), true);
            grad.addColour(0.45, juce::Colour(0xffffb067));
            g.setGradientFill(grad);
            g.fillEllipse(r);
        }
        else
        {
            g.setColour(juce::Colours::black.withAlpha(0.7f));
            g.fillEllipse(r.expanded(1.0f));
            juce::ColourGradient grad(colour::lampOff, r.getX() + d * 0.35f, r.getY() + d * 0.3f, juce::Colour(0xff1c0d09), r.getRight(), r.getBottom(), true);
            g.setGradientFill(grad);
            g.fillEllipse(r);
        }
    }

    void drawKey(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, bool on, bool lens, bool enabled, bool hot)
    {
        juce::Graphics::ScopedSaveState s(g);
        if (! enabled)
            g.beginTransparencyLayer(0.4f);
        constexpr float corner = 4.0f;
        g.setColour(juce::Colours::black.withAlpha(hot ? 0.0f : 0.5f));
        g.fillRoundedRectangle(area.translated(0.0f, 2.0f), corner);
        if (hot)
            glowRounded(g, area, corner, colour::orange, 4.0f, 0.3f);
        g.setColour(juce::Colour(0xff08090a));
        g.fillRoundedRectangle(area.expanded(1.0f), corner + 1.0f);
        if (hot)
            g.setGradientFill(vertical(area, { { 0.0f, juce::Colour(0xffffa25c) }, { 0.52f, colour::orange }, { 1.0f, juce::Colour(0xffc95a17) } }));
        else
            g.setGradientFill(vertical(area, { { 0.0f, juce::Colour(0xff3d3e43) }, { 0.55f, juce::Colour(0xff2b2c30) }, { 1.0f, juce::Colour(0xff232427) } }));
        g.fillRoundedRectangle(area, corner);
        g.setColour(juce::Colours::white.withAlpha(hot ? 0.4f : 0.14f));
        g.fillRect(area.getX() + corner, area.getY(), area.getWidth() - 2.0f * corner, 1.0f);
        g.setColour(juce::Colours::black.withAlpha(hot ? 0.15f : 0.35f));
        g.fillRect(area.getX() + corner * 0.5f, area.getBottom() - 2.0f, area.getWidth() - corner, 2.0f);

        const auto font = labelFont(10.5f);
        const auto t = text.toUpperCase();
        const float tw = t.isEmpty() ? 0.0f : juce::GlyphArrangement::getStringWidth(font, t);
        const float total = (lens ? 6.0f + (tw > 0.0f ? 5.0f : 0.0f) : 0.0f) + tw;
        float x = area.getCentreX() - total * 0.5f;
        if (lens)
        {
            drawLens(g, { x + 3.0f, area.getCentreY() }, on);
            x += 11.0f;
        }
        if (t.isNotEmpty())
        {
            g.setFont(font);
            // the engraving's shadow, then the name
            g.setColour(hot ? juce::Colours::white.withAlpha(0.25f) : juce::Colours::black.withAlpha(0.6f));
            g.drawText(t, juce::Rectangle<float>(x, area.getY() + (hot ? 1.0f : -1.0f), tw + 2.0f, area.getHeight()), juce::Justification::centredLeft, false);
            g.setColour(hot ? colour::orangeInk : on ? juce::Colours::white : colour::capInk);
            g.drawText(t, juce::Rectangle<float>(x, area.getY(), tw + 2.0f, area.getHeight()), juce::Justification::centredLeft, false);
        }
        if (! enabled)
            g.endTransparencyLayer();
    }

    void drawDisplay(juce::Graphics& g, juce::Rectangle<float> area)
    {
        constexpr float corner = 4.0f;
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.fillRoundedRectangle(area.translated(0.0f, 1.0f), corner);
        g.setGradientFill(vertical(area, { { 0.0f, juce::Colour(0xff0a0b0b) }, { 1.0f, juce::Colour(0xff121314) } }));
        g.fillRoundedRectangle(area, corner);
        g.setColour(juce::Colours::black);
        g.fillRect(area.getX() + corner, area.getY(), area.getWidth() - 2.0f * corner, 1.5f);
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.drawRoundedRectangle(area.reduced(0.5f), corner, 1.0f);
        // the glass's sheen
        const auto glass = area.reduced(1.0f);
        juce::ColourGradient sheen(juce::Colours::white.withAlpha(0.07f), glass.getX(), glass.getY(),
                                   juce::Colours::white.withAlpha(0.0f), glass.getX() + glass.getWidth() * 0.25f, glass.getY() + glass.getHeight() * 1.2f, false);
        g.setGradientFill(sheen);
        g.fillRoundedRectangle(glass, corner - 1.0f);
    }

    void drawWell(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, juce::Colour ink)
    {
        drawDisplay(g, area);
        const auto font = fontOf(Face::monoBold, 11.5f);
        g.setFont(font);
        g.setColour(ink.withAlpha(0.22f));
        g.drawText(text, area.translated(0.0f, 0.5f).expanded(0.6f, 0.0f), juce::Justification::centred, false);
        g.setColour(ink);
        g.drawText(text, area, juce::Justification::centred, false);
    }

    void drawScreen(juce::Graphics& g, juce::Rectangle<float> area, float corner)
    {
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.fillRoundedRectangle(area.translated(0.0f, 1.0f), corner);
        g.setColour(juce::Colour(0xff050506));
        g.fillRoundedRectangle(area, corner);
        g.setGradientFill(vertical(area, { { 0.0f, juce::Colour(0xff0f1012) }, { 1.0f, juce::Colour(0xff141518) } }));
        g.fillRoundedRectangle(area.reduced(1.0f), corner - 1.0f);
        // the shadow its rim casts inside
        g.setGradientFill(vertical(area.withHeight(8.0f), { { 0.0f, juce::Colours::black.withAlpha(0.7f) }, { 1.0f, juce::Colours::black.withAlpha(0.0f) } }));
        g.fillRoundedRectangle(area.reduced(1.0f).withHeight(8.0f), corner - 1.0f);
    }

    void drawCell(juce::Graphics& g, juce::Rectangle<float> r, Cell look, float alpha)
    {
        constexpr float corner = 3.0f;
        juce::Graphics::ScopedSaveState s(g);
        if (alpha < 0.999f)
            g.beginTransparencyLayer(alpha);
        if (look == Cell::hot)
        {
            glowRounded(g, r, corner, colour::orange, 4.0f, 0.35f);
            juce::ColourGradient grad(juce::Colour(0xffffc489), r.getCentreX(), r.getY() + r.getHeight() * 0.3f, juce::Colour(0xffcc5c18),
                                      r.getRight(), r.getBottom() + r.getHeight() * 0.4f, true);
            grad.addColour(0.6, colour::orange);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(r, corner);
            g.setColour(juce::Colours::white.withAlpha(0.35f));
            g.fillRect(r.getX() + corner, r.getY(), r.getWidth() - 2.0f * corner, 1.0f);
        }
        else
        {
            g.setColour(juce::Colours::black);
            g.fillRoundedRectangle(r.translated(0.0f, 1.0f), corner);
            const auto [a, b] = look == Cell::lit ? std::pair { juce::Colour(0xff55575c), juce::Colour(0xff44464b) }
                              : look == Cell::half ? std::pair { juce::Colour(0xff36373b), juce::Colour(0xff2e2f33) }
                                                   : std::pair { colour::cell, colour::cellOff };
            g.setGradientFill(vertical(r, { { 0.0f, a }, { 1.0f, b } }));
            g.fillRoundedRectangle(r, corner);
            g.setColour(juce::Colours::white.withAlpha(look == Cell::lit ? 0.12f : 0.05f));
            g.fillRect(r.getX() + corner, r.getY(), r.getWidth() - 2.0f * corner, 1.0f);
        }
        if (alpha < 0.999f)
            g.endTransparencyLayer();
    }
}
