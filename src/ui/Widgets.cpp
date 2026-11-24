#include "ui/Widgets.h"

#include "ui/Theme.h"

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

    void Knob::paint(juce::Graphics& g)
    {
        const float value = get ? juce::jlimit(0.0f, 1.0f, get()) : 0.0f;
        const float followed = follow ? follow() : -1.0f;
        const float shown = followed >= 0.0f ? followed : value;
        const bool isDim = dimmed && dimmed();
        const juce::Point<float> c(getWidth() * 0.5f, tickRoom + size * 0.5f);
        const float r = size * 0.5f;

        // the ticks: eleven round the sweep
        g.setColour(juce::Colour(0xffb5b6b9).withAlpha(isDim ? 0.4f : 0.9f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = -sweep * 0.5f + sweep * i / 10.0f;
            const juce::Point<float> dir(std::sin(a), -std::cos(a));
            const float len = (i == 0 || i == 5 || i == 10) ? 4.0f : 2.5f;
            g.drawLine(juce::Line<float>(c + dir * (r + 4.0f), c + dir * (r + 4.0f + len)), 1.1f);
        }
        // the body
        const auto body = juce::Rectangle<float>(size, size).withCentre(c);
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillEllipse(body.translated(0.0f, 2.0f).expanded(1.0f));
        if (accentRim)
        {
            juce::ColourGradient grad(juce::Colour(0xfff7a463), c.x - r * 0.4f, c.y - r * 0.6f, juce::Colour(0xffc4561a), c.x + r * 0.5f, c.y + r, true);
            g.setGradientFill(grad);
        }
        else
        {
            juce::ColourGradient grad(juce::Colour(0xff3a3b3e), c.x - r * 0.4f, c.y - r * 0.6f, juce::Colour(0xff0c0c0d), c.x + r * 0.5f, c.y + r, true);
            g.setGradientFill(grad);
        }
        g.fillEllipse(body);
        g.setColour(juce::Colours::white.withAlpha(0.07f));
        g.drawEllipse(body.reduced(r * 0.32f), 1.0f);
        g.setColour(juce::Colours::black);
        g.drawEllipse(body, 1.0f);
        // the pointer
        const float a = -sweep * 0.5f + sweep * shown;
        const juce::Point<float> dir(std::sin(a), -std::cos(a));
        g.setColour(accentRim ? juce::Colour(0xff1a120c) : (isDim ? colour::orange.withAlpha(0.45f) : colour::orange));
        g.drawLine(juce::Line<float>(c + dir * (r * 0.18f), c + dir * (r * 0.9f)), size >= 50.0f ? 3.0f : 2.4f);
        if (followed >= 0.0f && std::abs(followed - value) > 0.004f)
        {
            // where the knob itself stands, while the pattern moves it
            const float a0 = -sweep * 0.5f + sweep * value;
            const juce::Point<float> d0(std::sin(a0), -std::cos(a0));
            g.setColour(juce::Colours::white.withAlpha(0.6f));
            g.fillEllipse(juce::Rectangle<float>(3.0f, 3.0f).withCentre(c + d0 * (r + 2.0f)));
        }
        float y = c.y + r + 6.0f;
        if (showRange)
        {
            g.setColour(juce::Colour(0xff8d8f94));
            g.setFont(monoFont(9.0f));
            g.drawText("0", juce::Rectangle<float>(c.x - r - 6.0f, y - 6.0f, 12.0f, 10.0f), juce::Justification::centred, false);
            g.drawText("10", juce::Rectangle<float>(c.x + r - 8.0f, y - 6.0f, 16.0f, 10.0f), juce::Justification::centred, false);
            y += 4.0f;
        }
        if (name.isNotEmpty())
        {
            g.setColour(colour::label);
            g.setFont(labelFont(11.0f));
            g.drawText(name.toUpperCase(), juce::Rectangle<float>(0.0f, y, static_cast<float>(getWidth()), 14.0f), juce::Justification::centred, false);
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
        drawKey(g, area, textOf ? textOf() : text, isOn && isOn(), lamp, ! enabled || enabled());
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
        g.setColour(juce::Colour(0xff4b4d52));
        for (float y = 10.0f; y < trackH; y += 11.0f)
            g.drawHorizontalLine(juce::roundToInt(y), cx - 13.0f, cx + 13.0f);
        g.setColour(juce::Colour(0xff0e0f10));
        g.fillRect(juce::Rectangle<float>(cx - 2.0f, 0.0f, 4.0f, trackH));
        const float capY = (1.0f - v) * (trackH - 14.0f);
        const juce::Rectangle<float> cap(cx - 12.0f, capY, 24.0f, 14.0f);
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillRoundedRectangle(cap.translated(0.0f, 2.0f), 2.0f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff3b3c40), cap.getX(), cap.getY(), juce::Colour(0xff16171a), cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle(cap, 2.0f);
        g.setColour(colour::orange);
        g.fillRect(cap.getX() + 3.0f, cap.getY() + 6.0f, cap.getWidth() - 6.0f, 2.0f);
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
