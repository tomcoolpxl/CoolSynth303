#include "ui/Keyboard.h"

#include "Editor.h"
#include "Processor.h"
#include "ui/Theme.h"

namespace cs303::ui
{
    namespace
    {
        constexpr int top = 8; // the section's line sits this far down, its name set into it
        constexpr bool isWhite(int key) noexcept
        {
            const int n = key % 12;
            return n == 0 || n == 2 || n == 4 || n == 5 || n == 7 || n == 9 || n == 11;
        }
        constexpr const char* letters[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    }

    // ---------------------------------------------------------------- the piano
    Piano::Piano(Editor& e) : editor(e) {}

    juce::Rectangle<float> Piano::keyBounds(int key) const
    {
        int whitesBefore = 0;
        for (int k = 0; k < key; ++k)
            whitesBefore += isWhite(k) ? 1 : 0;
        if (isWhite(key))
            return { static_cast<float>(whitesBefore * whiteWidth), 0.0f, static_cast<float>(whiteWidth), static_cast<float>(getHeight()) };
        return { static_cast<float>(whitesBefore * whiteWidth - 9), 0.0f, 18.0f, getHeight() * 0.62f };
    }

    int Piano::keyAt(juce::Point<float> p) const
    {
        for (int k = 0; k < keys; ++k)
            if (! isWhite(k) && keyBounds(k).contains(p))
                return k;
        for (int k = 0; k < keys; ++k)
            if (isWhite(k) && keyBounds(k).contains(p))
                return k;
        return -1;
    }

    void Piano::paint(juce::Graphics& g)
    {
        auto& p = editor.processor;
        const int sounding = p.isPlaying() || held >= 0 ? p.soundingKey() : -1;
        const auto& step = editor.edited().step[static_cast<size_t>(editor.pick)];
        const int picked = ! p.isPlaying() && ! editor.transposeMode && step.gate == Gate::note ? step.key() : -1;
        const int transposed = editor.transposeMode ? 12 + p.transposition() : -1;
        const auto drawOne = [&](int k) {
            const auto r = keyBounds(k);
            const bool white = isWhite(k);
            const bool hit = editor.transposeMode ? k == transposed : k == sounding;
            const bool pick = k == picked;
            if (white)
            {
                // ivory, a shadow along its foot and its right edge; the picked key warm, the one sounding orange
                const auto body = r.withTrimmedTop(-4.0f).withTrimmedBottom(2.0f).reduced(0.5f, 0.0f);
                g.setColour(juce::Colours::black.withAlpha(0.5f));
                g.fillRoundedRectangle(body.translated(0.0f, 2.0f), 4.0f);
                juce::ColourGradient grad(juce::Colour(0xfff7f5ef), 0.0f, body.getY(), juce::Colour(0xffcfccc3), 0.0f, body.getBottom(), false);
                grad.addColour(0.7, juce::Colour(0xffe6e3da));
                if (hit)
                    grad = juce::ColourGradient(juce::Colour(0xffffb67a), 0.0f, body.getY(), colour::orange, 0.0f, body.getBottom(), false);
                else if (pick)
                {
                    grad = juce::ColourGradient(juce::Colour(0xfffff8f0), 0.0f, body.getY(), juce::Colour(0xfff0a065), 0.0f, body.getBottom(), false);
                    grad.addColour(0.7, juce::Colour(0xffffd9b5));
                }
                g.setGradientFill(grad);
                g.fillRoundedRectangle(body, 4.0f);
                g.setColour(juce::Colours::black.withAlpha(0.12f));
                g.fillRect(body.withTop(body.getBottom() - 3.0f).reduced(2.0f, 0.0f));
                g.setColour(juce::Colours::black.withAlpha(0.25f));
                g.fillRect(body.getRight() - 1.0f, body.getY(), 1.0f, body.getHeight() - 2.0f);
                g.setColour(hit ? colour::orangeInk : pick ? juce::Colour(0xff8a3e10) : juce::Colour(0xff8a877e));
                g.setFont(labelFont(9.5f));
                g.drawText(letters[k % 12], r.withTop(r.getBottom() - 20.0f), juce::Justification::centred, false);
            }
            else
            {
                // gloss black, lit along its middle, a lip at its foot
                const auto body = r.withTrimmedTop(-4.0f);
                g.setColour(juce::Colours::black.withAlpha(0.6f));
                g.fillRoundedRectangle(body.translated(0.0f, 3.0f), 3.0f);
                juce::ColourGradient grad(juce::Colour(0xff0d0d0e), body.getX(), 0.0f, juce::Colour(0xff111213), body.getRight(), 0.0f, false);
                grad.addColour(0.45, juce::Colour(0xff2b2c30));
                if (hit || pick)
                {
                    const auto mid = hit ? colour::orange : juce::Colour(0xff8a4a20);
                    grad = juce::ColourGradient(mid.darker(0.6f), body.getX(), 0.0f, mid.darker(0.5f), body.getRight(), 0.0f, false);
                    grad.addColour(0.45, mid);
                }
                g.setGradientFill(grad);
                g.fillRoundedRectangle(body, 3.0f);
                g.setColour(juce::Colour(0xff050505));
                g.fillRect(body.withTop(body.getBottom() - 5.0f).reduced(1.0f, 0.0f));
                g.setColour(juce::Colours::white.withAlpha(0.12f));
                g.fillRect(body.getX() + 1.0f, body.getBottom() - 6.0f, body.getWidth() - 2.0f, 1.0f);
            }
            if (pick && white)
            {
                // the picked key glows from below
                g.setColour(colour::orange.withAlpha(0.35f));
                g.fillRect(r.withTop(r.getBottom() - 4.0f).reduced(2.0f, 0.0f));
            }
        };
        for (int k = 0; k < keys; ++k)
            if (isWhite(k))
                drawOne(k);
        for (int k = 0; k < keys; ++k)
            if (! isWhite(k))
                drawOne(k);
    }

    void Piano::mouseDown(const juce::MouseEvent& e)
    {
        const int k = keyAt(e.position);
        if (k < 0)
            return;
        held = k;
        if (! editor.processor.isPlaying())
            editor.processor.audition(k);
        editor.playKey(k);
    }

    void Piano::mouseUp(const juce::MouseEvent&)
    {
        if (held >= 0)
            editor.processor.audition(-1);
        held = -1;
        repaint();
    }

    // ---------------------------------------------------------------- the section
    KeyboardSection::KeyboardSection(Editor& e) : editor(e)
    {
        auto& p = editor.processor;
        auto& accent = key("Accent", 18, 28, 62);
        accent.isOn = [this] { return editor.edited().step[static_cast<size_t>(editor.pick)].accent; };
        accent.onClick = [this] { editor.togglePick(true); };
        auto& slide = key("Slide", 18, 76, 62);
        slide.isOn = [this] { return editor.edited().step[static_cast<size_t>(editor.pick)].slide; };
        slide.onClick = [this] { editor.togglePick(false); };

        piano = std::make_unique<Piano>(editor);
        piano->setBounds(104, top + 16, Piano::whites * Piano::whiteWidth, 80);
        addAndMakeVisible(*piano);

        const int rx = 104 + Piano::whites * Piano::whiteWidth + 34;
        key("Back", rx, 28, 62).onClick = [this] { editor.movePick(-1); };
        key("Step", rx + 70, 28, 62).onClick = [this] { editor.movePick(1); };
        key("Tie", rx, 76, 62).onClick = [this] { editor.setGateAtPick(Gate::tie); };
        key("Rest", rx + 70, 76, 62).onClick = [this] { editor.setGateAtPick(Gate::rest); };
        auto& write = key("Write", rx + 160, 28, 86);
        write.isOn = [this] { return ! editor.transposeMode; };
        write.onClick = [this] { editor.transposeMode = false; editor.refreshAll(); };
        auto& trans = key("Transpose", rx + 160, 76, 86);
        trans.isOn = [this] { return editor.transposeMode; };
        trans.onClick = [this] { editor.transposeMode = true; editor.refreshAll(); };
        trans.onRightClick = [this, &p = p] { p.setTranspose(0); editor.refreshAll(); };

        auto hint = std::make_unique<Caption>();
        hint->multiline = true;
        hint->font = bodyFont(11.5f);
        hint->ink = colour::hint;
        hint->justification = juce::Justification::topLeft;
        hint->text = [this, &p = p] {
            if (editor.transposeMode)
                return juce::String("Transpose: a key shifts the running pattern; the middle C plays it as written.")
                       + (p.transposition() != 0 ? juce::String(" Now ") + (p.transposition() > 0 ? "+" : "") + juce::String(p.transposition()) + "." : juce::String());
            return "Write: a key sets step " + juce::String(editor.pick + 1) + "'s note and octave, then moves on.";
        };
        hint->setBounds(rx + 266, top + 24, 1196 - (rx + 266), 80);
        addAndMakeVisible(*hint);
        parts.push_back(std::move(hint));

        // the octaves named under the keys
        const std::pair<const char*, std::pair<int, int>> octaves[] = { { "Octave down", { 0, 7 } }, { "As written", { 7, 7 } }, { "Octave up", { 14, 8 } } };
        for (const auto& [name, span] : octaves)
        {
            auto c = std::make_unique<Caption>();
            const juce::String text(name);
            c->text = [text] { return text.toUpperCase(); };
            c->font = labelFont(9.5f).withExtraKerningFactor(0.1f);
            c->ink = juce::Colour(0xff77797e);
            c->justification = juce::Justification::centredBottom;
            c->setBounds(104 + span.first * Piano::whiteWidth + 3, top + 100, span.second * Piano::whiteWidth - 6, 14);
            addAndMakeVisible(*c);
            parts.push_back(std::move(c));
        }
    }

    Key& KeyboardSection::key(const juce::String& text, int x, int y, int w)
    {
        auto k = std::make_unique<Key>(text, true);
        k->setBounds(x, top + y, w, 24 + Key::lampRoom);
        addAndMakeVisible(*k);
        auto& ref = *k;
        parts.push_back(std::move(k));
        return ref;
    }

    void KeyboardSection::paint(juce::Graphics& g)
    {
        drawSection(g, juce::Rectangle<int>(0, top, getWidth(), getHeight() - top).toFloat(), "Keyboard", colour::face);
        // the keys' bed
        drawScreen(g, juce::Rectangle<float>(104.0f - 6.0f, top + 12.0f, Piano::whites * Piano::whiteWidth + 12.0f, 88.0f), 5.0f);
        // the line over each octave's name, fading at its ends
        for (const auto& [from, n] : { std::pair { 0, 7 }, std::pair { 7, 7 }, std::pair { 14, 8 } })
        {
            const float x0 = 104.0f + from * Piano::whiteWidth + 3.0f, x1 = 104.0f + (from + n) * Piano::whiteWidth - 3.0f;
            juce::ColourGradient line(juce::Colour(0x004a4c51), x0, 0.0f, juce::Colour(0x004a4c51), x1, 0.0f, false);
            line.addColour(0.12, juce::Colour(0xff4a4c51));
            line.addColour(0.88, juce::Colour(0xff4a4c51));
            g.setGradientFill(line);
            g.fillRect(x0, static_cast<float>(top + 100) + 2.0f, x1 - x0, 1.0f);
        }
    }

    void KeyboardSection::refresh()
    {
        piano->repaint();
        for (auto& c : parts)
            c->repaint();
    }
}
