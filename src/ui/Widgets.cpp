#include "ui/Widgets.h"

#include "ui/Theme.h"

#include <vector>

namespace cs303::ui
{
    namespace
    {
        constexpr float startAngle = juce::MathConstants<float>::pi * 1.25f;  // 0: down-left (from 12 o'clock, clockwise: -135 degrees)
        constexpr float sweep = juce::MathConstants<float>::pi * 1.5f;        // 270 degrees
        constexpr int tickRoom = 12;
    }

    // ---------------------------------------------------------------- Knob
    Knob::Knob(juce::String n, float d) : name(std::move(n)), size(d) {}

    juce::Rectangle<int> Knob::preferredBounds(int x, int y) const
    {
        const int w = juce::roundToInt(size) + 2 * tickRoom + 12;
        const int h = juce::roundToInt(size) + 2 * tickRoom + (showRange ? 6 : 0) + (name.isNotEmpty() ? 14 : 0);
        return { x, y, w, h };
    }

    namespace
    {
        /** Fills a disc with colours turning round it (a conic gradient, which JUCE does not have): thin wedges, each
            the colour at its middle. */
        void fillTurned(juce::Graphics& g, juce::Rectangle<float> disc, float from, std::initializer_list<std::pair<float, juce::Colour>> stops, int wedges = 72)
        {
            const std::vector<std::pair<float, juce::Colour>> s(stops);
            const auto at = [&s](float t) {
                for (size_t i = 1; i < s.size(); ++i)
                    if (t <= s[i].first)
                        return s[i - 1].second.interpolatedWith(s[i].second, (t - s[i - 1].first) / (s[i].first - s[i - 1].first));
                return s.back().second;
            };
            const float step = juce::MathConstants<float>::twoPi / static_cast<float>(wedges);
            for (int i = 0; i < wedges; ++i)
            {
                juce::Path p;
                const float a0 = from + step * static_cast<float>(i);
                p.addPieSegment(disc, a0, a0 + step * 1.06f, 0.0f); // a hair of overlap, no seams
                g.setColour(at((static_cast<float>(i) + 0.5f) / static_cast<float>(wedges)));
                g.fillPath(p);
            }
        }
    }

    void Knob::paint(juce::Graphics& g)
    {
        const float value = get ? juce::jlimit(0.0f, 1.0f, get()) : 0.0f;
        const float followed = follow ? follow() : -1.0f;
        const float shown = followed >= 0.0f ? followed : value;
        const bool isDim = dimmed && dimmed();
        const juce::Point<float> c(getWidth() * 0.5f, tickRoom + size * 0.5f);
        const float r = size * 0.5f;
        const float start = -sweep * 0.5f;

        // the value as a lit arc on a dark track
        const float ar = r + 6.0f;
        {
            juce::Path track;
            track.addCentredArc(c.x, c.y, ar, ar, 0.0f, start, start + sweep, true);
            g.setColour(juce::Colours::white.withAlpha(0.06f));
            g.strokePath(track, juce::PathStrokeType(1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::translation(0.0f, 1.5f));
            g.setColour(juce::Colour(0xff0a0a0b));
            g.strokePath(track, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            if (shown > 0.005f)
            {
                juce::Path lit;
                lit.addCentredArc(c.x, c.y, ar, ar, 0.0f, start, start + sweep * shown, true);
                const auto arc = (accentRim ? colour::accentNote : colour::orange).withMultipliedAlpha(isDim ? 0.4f : 1.0f);
                g.setColour(arc.withMultipliedAlpha(0.25f));
                g.strokePath(lit, juce::PathStrokeType(5.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour(arc);
                g.strokePath(lit, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            if (followed >= 0.0f && std::abs(followed - value) > 0.004f)
            {
                // where the knob itself stands, while the pattern moves it
                const float a0 = start + sweep * value;
                g.setColour(juce::Colours::white.withAlpha(0.85f));
                g.fillEllipse(juce::Rectangle<float>(3.5f, 3.5f).withCentre(c + juce::Point<float>(std::sin(a0), -std::cos(a0)) * ar));
            }
        }
        // the skirt: knurled, lit from above
        const auto body = juce::Rectangle<float>(size, size).withCentre(c);
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillEllipse(body.translated(0.0f, 3.0f).expanded(0.5f));
        g.setColour(juce::Colours::black);
        g.fillEllipse(body.expanded(1.0f));
        {
            const auto [k0, k1] = accentRim ? std::pair { juce::Colour(0xff3a1a08), juce::Colour(0xff5a2a0e) } : std::pair { juce::Colour(0xff0d0d0e), juce::Colour(0xff232427) };
            g.setColour(k0);
            g.fillEllipse(body);
            const int ridges = 45;
            const float step = juce::MathConstants<float>::twoPi / static_cast<float>(ridges);
            g.setColour(k1);
            for (int i = 0; i < ridges; ++i)
            {
                juce::Path p;
                p.addPieSegment(body, step * static_cast<float>(i), step * (static_cast<float>(i) + 0.5f), 0.0f);
                g.fillPath(p);
            }
            juce::ColourGradient sheen(juce::Colours::white.withAlpha(0.12f), c.x, body.getY() + size * 0.3f, juce::Colours::white.withAlpha(0.0f), c.x, body.getY() + size * 0.9f, true);
            g.setGradientFill(sheen);
            g.fillEllipse(body);
            g.setColour(juce::Colours::white.withAlpha(0.05f));
            g.drawEllipse(body.reduced(0.5f), 1.0f);
        }
        // the cap: turned aluminium, or orange for the accent's lock
        const float capD = size * 0.6f;
        const auto cap = juce::Rectangle<float>(capD, capD).withCentre(c);
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.fillEllipse(cap.translated(0.0f, 1.0f).expanded(1.0f));
        if (accentRim)
        {
            juce::ColourGradient grad(juce::Colour(0xffffd2a6), cap.getX() + capD * 0.4f, cap.getY() + capD * 0.3f, juce::Colour(0xffb8501a), cap.getRight(), cap.getBottom(), true);
            grad.addColour(0.6, colour::orange);
            g.setGradientFill(grad);
            g.fillEllipse(cap);
        }
        else
            fillTurned(g, cap, juce::MathConstants<float>::pi / 6.0f,
                       { { 0.0f, juce::Colour(0xff8e9196) }, { 0.1f, juce::Colour(0xffeef0f2) }, { 0.22f, juce::Colour(0xffa3a6ab) }, { 0.36f, juce::Colour(0xffe4e6e9) },
                         { 0.5f, juce::Colour(0xff8e9196) }, { 0.62f, juce::Colour(0xfff1f2f4) }, { 0.75f, juce::Colour(0xff9fa2a7) }, { 0.88f, juce::Colour(0xffe1e3e6) },
                         { 1.0f, juce::Colour(0xff8e9196) } });
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.drawEllipse(cap, 1.0f);
        {
            juce::Path rim;
            rim.addCentredArc(c.x, c.y, capD * 0.5f - 0.8f, capD * 0.5f - 0.8f, 0.0f, -1.2f, 1.2f, true);
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.strokePath(rim, juce::PathStrokeType(0.8f));
        }
        // the pointer: an orange line across the skirt, a notch in the cap
        const float a = start + sweep * shown;
        const juce::Point<float> dir(std::sin(a), -std::cos(a));
        g.setColour((accentRim ? juce::Colour(0xfffff2e4) : colour::orange).withMultipliedAlpha(isDim ? 0.5f : 1.0f));
        g.drawLine(juce::Line<float>(c + dir * (capD * 0.5f), c + dir * (r - 1.5f)), 2.6f);
        g.setColour(juce::Colour(0xff141416).withAlpha(0.75f));
        g.drawLine(juce::Line<float>(c + dir * (capD * 0.12f), c + dir * (capD * 0.42f)), 1.6f);

        // the name: the arc shows the value, so no 0 and 10 under it
        if (name.isNotEmpty())
        {
            g.setColour(colour::label);
            g.setFont(labelFont(11.0f));
            g.drawText(name.toUpperCase(), juce::Rectangle<float>(0.0f, c.y + r + 8.0f, static_cast<float>(getWidth()), 14.0f), juce::Justification::centred, false);
        }
    }

    void Knob::change(float v)
    {
        if (set)
            set(juce::jlimit(0.0f, 1.0f, v));
        repaint();
    }

    void Knob::mouseDown(const juce::MouseEvent& e)
    {
        dragStartValue = get ? get() : 0.0f;
        if (gestureStart)
            gestureStart();
        juce::ignoreUnused(e);
    }

    void Knob::mouseDrag(const juce::MouseEvent& e)
    {
        const float pixels = e.mods.isShiftDown() ? 800.0f : 180.0f;
        change(dragStartValue - static_cast<float>(e.getDistanceFromDragStartY()) / pixels);
    }

    void Knob::mouseUp(const juce::MouseEvent&)
    {
        if (gestureEnd)
            gestureEnd();
    }

    void Knob::mouseDoubleClick(const juce::MouseEvent&)
    {
        if (gestureStart)
            gestureStart();
        change(defaultValue);
        if (gestureEnd)
            gestureEnd();
    }

    void Knob::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w)
    {
        if (gestureStart)
            gestureStart();
        change((get ? get() : 0.0f) + (w.deltaY > 0.0f ? 0.02f : -0.02f));
        if (gestureEnd)
            gestureEnd();
    }

    // ---------------------------------------------------------------- Key
    Key::Key(juce::String t, bool hasLamp) : lamp(hasLamp), text(std::move(t)) {}

    void Key::paint(juce::Graphics& g)
    {
        auto area = getLocalBounds().toFloat();
        if (lamp)
            area.removeFromTop(static_cast<float>(lampRoom));
        area = area.reduced(1.0f, 0.0f).withTrimmedBottom(2.0f); // room for the cap's edge and its shadow
        const bool on = isOn && isOn();
        const bool lens = isOn != nullptr; // a key that can be on carries a lens
        drawKey(g, area, glyph != 0 ? juce::String() : textOf ? textOf() : text, on, lens && glyph == 0, ! enabled || enabled(), hot);
        if (glyph != 0)
        {
            // a waveform drawn on the key, beside its lens
            const float w = 20.0f, h = 10.0f;
            const float left = area.getCentreX() - (6.0f + 5.0f + w) * 0.5f;
            drawLens(g, { left + 3.0f, area.getCentreY() }, on);
            const float x = left + 11.0f, y = area.getCentreY() - h * 0.5f;
            juce::Path p;
            if (glyph == 1)
            {
                p.startNewSubPath(x, y + h);
                p.lineTo(x + w * 0.5f, y);
                p.lineTo(x + w * 0.5f, y + h);
                p.lineTo(x + w, y);
                p.lineTo(x + w, y + h);
            }
            else
            {
                p.startNewSubPath(x, y + h);
                p.lineTo(x, y);
                p.lineTo(x + w / 3.0f, y);
                p.lineTo(x + w / 3.0f, y + h);
                p.lineTo(x + 2.0f * w / 3.0f, y + h);
                p.lineTo(x + 2.0f * w / 3.0f, y);
                p.lineTo(x + w, y);
                p.lineTo(x + w, y + h);
            }
            g.setColour(on ? juce::Colours::white : juce::Colour(0xffc9c8c3));
            g.strokePath(p, juce::PathStrokeType(1.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::square));
        }
    }

    void Key::mouseDown(const juce::MouseEvent& e)
    {
        if (enabled && ! enabled())
            return;
        if (e.mods.isPopupMenu())
        {
            if (onRightClick)
                onRightClick();
        }
        else if (onClick)
            onClick();
        repaint();
    }

    // ---------------------------------------------------------------- Fader
    Fader::Fader(juce::String n) : name(std::move(n)) {}

    void Fader::paint(juce::Graphics& g)
    {
        const float v = get ? juce::jlimit(0.0f, 1.0f, get()) : 0.0f;
        const float trackH = 112.0f, cx = getWidth() * 0.5f;
        // the scale on both sides, longer every fifth
        g.setColour(juce::Colour(0xff4a4c51));
        for (int i = 0; i <= 10; ++i)
        {
            const float y = 10.0f + static_cast<float>(i) * 9.2f;
            const bool major = i % 5 == 0;
            g.fillRect(cx - (major ? 16.0f : 13.0f), y, major ? 7.0f : 4.0f, 1.0f);
            g.fillRect(cx + 9.0f, y, major ? 7.0f : 4.0f, 1.0f);
        }
        // the slot, lit below the cap
        const auto slot = juce::Rectangle<float>(cx - 4.0f, 2.0f, 8.0f, trackH - 4.0f);
        g.setColour(juce::Colour(0xff09090a));
        g.fillRoundedRectangle(slot, 4.0f);
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.drawRoundedRectangle(slot.reduced(0.5f), 4.0f, 1.0f);
        const float capY = 2.0f + (1.0f - v) * (trackH - 20.0f);
        const auto fill = juce::Rectangle<float>(cx - 1.5f, capY + 9.0f, 3.0f, slot.getBottom() - capY - 9.0f);
        if (fill.getHeight() > 0.0f)
        {
            glowRounded(g, fill, 1.5f, colour::orange, 3.0f, 0.4f);
            g.setColour(colour::orange);
            g.fillRoundedRectangle(fill, 1.5f);
        }
        // the cap: aluminium, an orange line across it
        const juce::Rectangle<float> cap(cx - 12.0f, capY, 24.0f, 16.0f);
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillRoundedRectangle(cap.translated(0.0f, 2.0f), 3.0f);
        juce::ColourGradient metal(juce::Colour(0xfff1f2f4), cap.getX(), cap.getY(), juce::Colour(0xff8c8f94), cap.getX(), cap.getBottom(), false);
        metal.addColour(0.5, juce::Colour(0xffb6b9be));
        g.setGradientFill(metal);
        g.fillRoundedRectangle(cap, 3.0f);
        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(cap, 3.0f, 0.8f);
        g.setColour(colour::orange);
        g.fillRoundedRectangle(cap.getX() + 3.0f, cap.getY() + 7.0f, cap.getWidth() - 6.0f, 2.0f, 1.0f);
        g.setColour(colour::label);
        g.setFont(labelFont(11.0f));
        g.drawText(name.toUpperCase(), juce::Rectangle<float>(-10.0f, trackH + 6.0f, getWidth() + 20.0f, 14.0f), juce::Justification::centred, false);
    }

    void Fader::mouseDown(const juce::MouseEvent&)
    {
        dragStartValue = get ? get() : 0.0f;
        if (gestureStart)
            gestureStart();
    }

    void Fader::mouseDrag(const juce::MouseEvent& e)
    {
        if (set)
            set(juce::jlimit(0.0f, 1.0f, dragStartValue - static_cast<float>(e.getDistanceFromDragStartY()) / 98.0f));
        repaint();
    }

    void Fader::mouseUp(const juce::MouseEvent&)
    {
        if (gestureEnd)
            gestureEnd();
    }

    // ---------------------------------------------------------------- ValueBox, Caption
    void ValueBox::paint(juce::Graphics& g)
    {
        drawWell(g, getLocalBounds().toFloat(), text ? text() : juce::String(), ink);
    }

    void ValueBox::mouseDown(const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu())
        {
            if (onRightClick)
                onRightClick();
        }
        else if (onClick)
            onClick();
        repaint();
    }

    void ValueBox::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w)
    {
        if (onWheel)
            onWheel(w.deltaY > 0.0f ? 1 : -1);
        repaint();
    }

    void Caption::paint(juce::Graphics& g)
    {
        g.setColour(ink);
        g.setFont(font);
        const auto t = text ? text() : juce::String();
        if (multiline)
        {
            juce::AttributedString s;
            s.append(t, font, ink);
            s.setJustification(justification);
            s.setLineSpacing(3.0f);
            s.draw(g, getLocalBounds().toFloat());
        }
        else
            g.drawText(t, getLocalBounds(), justification, false);
    }
}
