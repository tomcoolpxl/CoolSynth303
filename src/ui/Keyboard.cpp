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
                if (k == hovered && ! hit)
                {
                    g.setColour(colour::orange.withAlpha(0.14f)); // under the mouse: a warm tint
                    g.fillRoundedRectangle(body, 4.0f);
                }
                g.setColour(juce::Colours::black.withAlpha(0.12f));
                g.fillRect(body.withTop(body.getBottom() - 3.0f).reduced(2.0f, 0.0f));
                g.setColour(juce::Colours::black.withAlpha(0.25f));
                g.fillRect(body.getRight() - 1.0f, body.getY(), 1.0f, body.getHeight() - 2.0f);
                g.setColour(hit ? colour::orangeInk : pick ? juce::Colour(0xff8a3e10) : juce::Colour(0xff8a877e));
                g.setFont(labelFont(9.5f));
                shapedText(g, letters[k % 12], r.withTop(r.getBottom() - 20.0f), juce::Justification::centred, false);
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
                if (k == hovered && ! hit)
                {
                    g.setColour(juce::Colours::white.withAlpha(0.16f));
                    g.fillRoundedRectangle(body, 3.0f);
                }
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

    juce::String Piano::hintAt(juce::Point<int>) const
    {
        return editor.transposeMode ? "Transpose: a key shifts the running pattern; the middle C plays it as written."
                                    : "Write: a key sets the picked step's note and octave, then moves on. The computer's keys Z to comma play the middle octave.";
    }

    juce::String KeyboardSection::hintAt(juce::Point<int> p) const
    {
        if (juce::Rectangle<int>(12, 26, 84, 22).contains(p))
            return "The picked step: its number and its note. Pick another in the grid, or with Back and Step.";
        return {};
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
        auto& accent = key("Accent", 12, 38, 84);
        accent.setHelpText("Accent on the picked step: louder, brighter, a shorter decay.");
        accent.isOn = [this] { return editor.edited().step[static_cast<size_t>(editor.pick)].accent; };
        accent.onClick = [this] { editor.togglePick(true); };
        auto& slide = key("Slide", 12, 66, 84);
        slide.setHelpText("Slide from the picked step into the next: the pitch glides and the note holds on.");
        slide.isOn = [this] { return editor.edited().step[static_cast<size_t>(editor.pick)].slide; };
        slide.onClick = [this] { editor.togglePick(false); };

        piano = std::make_unique<Piano>(editor);
        piano->setBounds(104, top + 16, Piano::whites * Piano::whiteWidth, 80);
        addAndMakeVisible(*piano);

        // layout B (CoolSynth's D350): the entry keys in two even rows, Write and Transpose one switch in a bay
        const int rx = 776;
        key("Back", rx, 38, 76).onClick = [this] { editor.movePick(-1); };
        key("Step", rx + 82, 38, 76).onClick = [this] { editor.movePick(1); };
        key("Tie", rx, 66, 76).onClick = [this] { editor.setGateAtPick(Gate::tie); };
        key("Rest", rx + 82, 66, 76).onClick = [this] { editor.setGateAtPick(Gate::rest); };
        auto& write = key("Write", modeBay.getX() + 3, 38, 84);
        write.isOn = [this] { return ! editor.transposeMode; };
        write.onClick = [this] { editor.transposeMode = false; editor.refreshAll(); };
        auto& trans = key("Transpose", modeBay.getX() + 89, 38, 84);
        trans.isOn = [this] { return editor.transposeMode; };
        trans.onClick = [this] { editor.transposeMode = true; editor.refreshAll(); };
        trans.onRightClick = [this, &p = p] { p.setTranspose(0); editor.refreshAll(); };

        auto hint = std::make_unique<Caption>();
        hint->multiline = true;
        hint->font = bodyFont(11.5f);
        hint->inkRole = &colour::hint;
        hint->justification = juce::Justification::topLeft;
        hint->text = [this, &p = p] {
            if (editor.transposeMode)
                return juce::String("Transpose: a key shifts the running pattern; the middle C plays it as written.")
                       + (p.transposition() != 0 ? juce::String(" Now ") + (p.transposition() > 0 ? "+" : "") + juce::String(p.transposition()) + "." : juce::String());
            return "Write: a key sets step " + juce::String(editor.pick + 1) + "'s note and octave, then moves on.";
        };
        hint->setBounds(modeBay.getX(), modeBay.getBottom() + 5, 250, 40);
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
            c->inkRole = &colour::dim;
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
        // the picked step, where its keys are: its number and its note
        {
            const auto r = juce::Rectangle<float>(12.0f, 26.0f, 84.0f, 22.0f);
            drawDisplay(g, r);
            const auto& st = editor.edited().step[static_cast<size_t>(editor.pick)];
            const juce::String note = st.gate == Gate::rest ? juce::String::charToString(0xb7)
                                    : st.gate == Gate::tie ? juce::String("tie")
                                    : juce::String(letters[st.note % 12]) + (st.note == 12 ? juce::String::charToString(0x2032) : juce::String())
                                          + (st.octave > 0 ? "+" : st.octave < 0 ? juce::String::charToString(0x2212) : juce::String());
            g.setColour(juce::Colour(0xff8a7a68));
            g.setFont(monoFont(9.5f));
            shapedText(g, "STEP " + juce::String(editor.pick + 1).paddedLeft('0', 2), r.withTrimmedLeft(8.0f), juce::Justification::centredLeft, false);
            g.setColour(colour::amber);
            g.setFont(fontOf(Face::monoBold, 13.5f));
            shapedText(g, note, r.withTrimmedRight(8.0f), juce::Justification::centredRight, false);
        }
        // the entry keys' and the switch's names, the switch's bay
        g.setColour(colour::dim);
        g.setFont(labelFont(9.5f).withExtraKerningFactor(0.14f));
        shapedText(g, "STEP ENTRY", juce::Rectangle<int>(776, 32, 158, 14), juce::Justification::centred, false);
        shapedText(g, "KEYS", juce::Rectangle<int>(modeBay.getX(), 32, modeBay.getWidth(), 14), juce::Justification::centred, false);
        drawScreen(g, modeBay.toFloat(), 5.0f);
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
        repaint(0, 0, 110, 60); // the picked step's display
        piano->repaint();
        for (auto& c : parts)
            c->repaint();
    }
}
