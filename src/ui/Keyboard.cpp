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
            if (hit)
                g.setGradientFill(juce::ColourGradient(juce::Colour(0xffffb67a), r.getX(), r.getY(), colour::orange, r.getX(), r.getBottom(), false));
            else if (white)
                g.setGradientFill(juce::ColourGradient(k % 12 == 0 ? juce::Colour(0xfff6f6f4) : juce::Colour(0xfff1f1ef), r.getX(), r.getY(),
                                                       k % 12 == 0 ? juce::Colour(0xffd6d7d9) : juce::Colour(0xffcfd0d2), r.getX(), r.getBottom(), false));
            else
                g.setGradientFill(juce::ColourGradient(juce::Colour(0xff3a3b3f), r.getX(), r.getY(), juce::Colour(0xff121315), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle(r.withTrimmedTop(-4.0f).reduced(white ? 0.5f : 0.0f, 0.0f), white ? 3.0f : 2.0f);
            g.setColour(juce::Colours::black.withAlpha(white ? 0.18f : 0.5f));
            g.fillRect(r.withTop(r.getBottom() - 3.0f).reduced(1.0f, 0.0f));
            g.setColour(juce::Colour(0xff111111));
            g.drawRoundedRectangle(r.withTrimmedTop(-4.0f).reduced(0.5f), white ? 3.0f : 2.0f, 1.0f);
            if (k == picked)
            {
                g.setColour(colour::orange);
                g.drawRoundedRectangle(r.reduced(white ? 2.0f : 1.5f), 2.0f, 2.0f);
            }
            if (white)
            {
                g.setColour(hit ? colour::orangeInk : juce::Colour(0xff4a4c51));
                g.setFont(labelFont(9.5f));
                g.drawText(letters[k % 12], r.withTop(r.getBottom() - 18.0f), juce::Justification::centred, false);
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
        trans.onRightClick = [this, &p] { p.setTranspose(0); editor.refreshAll(); };

        auto hint = std::make_unique<Caption>();
        hint->multiline = true;
        hint->font = bodyFont(11.5f);
        hint->ink = colour::hint;
        hint->justification = juce::Justification::topLeft;
        hint->text = [this, &p] {
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
            c->ink = juce::Colour(0xff8c8e93);
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
        // the line over each octave's name
        g.setColour(juce::Colour(0xff55575c));
        for (const auto& [from, n] : { std::pair { 0, 7 }, std::pair { 7, 7 }, std::pair { 14, 8 } })
            g.drawHorizontalLine(top + 100, 104.0f + from * Piano::whiteWidth + 3.0f, 104.0f + (from + n) * Piano::whiteWidth - 3.0f);
    }

    void KeyboardSection::refresh()
    {
        piano->repaint();
        for (auto& c : parts)
            c->repaint();
    }
}
