#include "ui/SequencerView.h"

#include "MidiExport.h"

#include "Editor.h"
#include "Processor.h"
#include "ui/Theme.h"

namespace cs303::ui
{
    using Knob_t = cs303::Knob;

    namespace
    {
        constexpr int top = 8; // the section's line, its name set into it
        constexpr const char* letters[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C" };
        juce::String two(int i) { return juce::String(i + 1).paddedLeft('0', 2); }

        /** A key of the pattern row and the chain: a dark cap, darker while its pattern is empty, orange while it
            plays. */
        void drawSlot(juce::Graphics& g, juce::Rectangle<float> r, bool has, bool hot, float alpha = 1.0f)
        {
            if (hot)
            {
                drawCell(g, r, Cell::hot, alpha);
                return;
            }
            juce::Graphics::ScopedSaveState s(g);
            if (alpha < 0.999f)
                g.beginTransparencyLayer(alpha);
            g.setColour(juce::Colour(0xff08090a));
            g.fillRoundedRectangle(r.expanded(1.0f), 4.0f);
            const auto upper = has ? palette().slotHasTop : palette().slotEmptyTop, bottom = has ? palette().slotHasBottom : palette().slotEmptyBottom;
            g.setGradientFill(juce::ColourGradient(upper, 0.0f, r.getY(), bottom, 0.0f, r.getBottom(), false));
            g.fillRoundedRectangle(r, 3.0f);
            g.setColour(juce::Colours::white.withAlpha(0.08f));
            g.fillRect(r.getX() + 3.0f, r.getY(), r.getWidth() - 6.0f, 1.0f);
            if (alpha < 0.999f)
                g.endTransparencyLayer();
        }

        /** Drag MIDI: dragged out, the edited pattern goes along as a MIDI file; clicked, it is saved as one. */
        class MidiKey final : public juce::Component
        {
        public:
            explicit MidiKey(Editor& e) : editor(e) {}

            void paint(juce::Graphics& g) override { drawKey(g, getLocalBounds().toFloat().reduced(1.0f, 0.0f).withTrimmedBottom(2.0f), "Drag MIDI", dragging, false); }

            void mouseDrag(const juce::MouseEvent& e) override
            {
                if (dragging || e.getDistanceFromDragStart() < 5)
                    return;
                const auto file = writePatternMidi(editor.edited(), editor.processor.project().bpm, fileName());
                if (! file.existsAsFile())
                    return;
                dragging = true;
                repaint();
                juce::DragAndDropContainer::performExternalDragDropOfFiles({ file.getFullPathName() }, false, this, [this] {
                    dragging = false;
                    repaint();
                });
            }

            void mouseUp(const juce::MouseEvent& e) override
            {
                if (dragging || e.mouseWasDraggedSinceMouseDown())
                    return;
                chooser = std::make_unique<juce::FileChooser>("Save the pattern as MIDI", juce::File::getSpecialLocation(juce::File::userMusicDirectory)
                                                                  .getChildFile(fileName() + ".mid"), "*.mid");
                chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this](const juce::FileChooser& c)
                {
                    auto f = c.getResult();
                    if (f == juce::File())
                        return;
                    f = f.withFileExtension("mid");
                    f.deleteFile();
                    juce::FileOutputStream out(f);
                    if (out.openedOk())
                        patternToMidi(editor.edited(), editor.processor.project().bpm).writeTo(out);
                });
            }

        private:
            Editor& editor;
            bool dragging = false;
            std::unique_ptr<juce::FileChooser> chooser;

            juce::String fileName() const
            {
                const auto& pat = editor.edited();
                return "CoolSynth303 " + two(editor.editPattern) + (editor.editB ? "B" : "A") + (pat.name.empty() ? juce::String() : " " + juce::String(pat.name));
            }
        };
    }

    // ---------------------------------------------------------------- the pattern row
    PatternRow::PatternRow(Editor& e) : editor(e)
    {
        // the pattern's length at the row's end: clicked through 16, 12 and 8, wheeled a step at a time
        length = std::make_unique<ValueBox>();
        length->text = [this] { return juce::String(editor.edited().length) + " steps"; };
        length->onClick = [this] { editor.editPatternWith([](Pattern& pat) { pat.length = pat.length == 16 ? 12 : pat.length == 12 ? 8 : 16; }); };
        length->onWheel = [this](int d) { editor.editPatternWith([d](Pattern& pat) { pat.length = juce::jlimit(1, 16, pat.length + d); }); };
        length->onRightClick = [this] { editor.editPatternWith([](Pattern& pat) { pat.length = 16; }); };
        length->setHelpText("The pattern's length: click for 16, 12 or 8 steps, wheel for any, right-click for 16.");
        length->setBounds(1114, rowY, 72, 26);
        addAndMakeVisible(*length);
    }

    // The places as the mockup's layout B gives them (coolsynth-303-relayout.src.html), from the row's left at x 72; the
    // bank's bay moved 10 px right so its edge meets the grid screen's below.
    juce::Rectangle<int> PatternRow::slotBounds(int i) const { return { 72 + i * 30 + (i / 4) * 6, rowY, 27, 26 }; }
    juce::Rectangle<int> PatternRow::variationBounds(bool b) const { return { 587 + (b ? 30 : 0), rowY, 28, 26 }; }
    juce::Rectangle<int> PatternRow::chainLabelBounds() const { return { 658, rowY, 60, 26 }; }
    juce::Rectangle<int> PatternRow::chainBounds(int i) const { return { 726 + i * 55, rowY + 1, 51, 24 }; }
    juce::Rectangle<int> PatternRow::addBounds() const { return { 956, rowY, 26, 26 }; }
    juce::Rectangle<int> PatternRow::loopBounds() const { return { 988, rowY, 66, 26 }; }
    int PatternRow::shownChain() const
    {
        const int n = static_cast<int>(editor.processor.project().chain.size());
        return n > maxShown ? maxShown - 1 : n;
    }

    void PatternRow::paint(juce::Graphics& g)
    {
        auto& p = editor.processor;
        const auto& proj = p.project();
        const bool playing = p.isPlaying();
        const auto font = monoFont(12.0f);
        // the bays: the bank, A and B, the chain's display; the names beside them
        drawScreen(g, juce::Rectangle<float>(68.0f, 0.0f, 504.0f, 32.0f), 5.0f); // its left edge on the grid screen's
        drawScreen(g, juce::Rectangle<float>(584.0f, 0.0f, 64.0f, 32.0f), 5.0f);
        drawDisplay(g, juce::Rectangle<float>(722.0f, 0.0f, 228.0f, 32.0f));
        g.setColour(colour::dim);
        g.setFont(labelFont(10.5f).withExtraKerningFactor(0.1f));
        shapedText(g, "PATTERN", juce::Rectangle<int>(4, rowY, 62, 26), juce::Justification::centredLeft, false);
        shapedText(g, "LENGTH", juce::Rectangle<int>(1064, rowY, 48, 26), juce::Justification::centredLeft, false);
        for (int i = 0; i < 16; ++i)
        {
            const auto r = slotBounds(i).toFloat();
            const auto notes = [](const Pattern& pat) {
                const Pattern blank;
                return pat.step != blank.step && std::any_of(pat.step.begin(), pat.step.end(), [](const Step& s) { return s.gate == Gate::note; });
            };
            const bool has = notes(proj.pattern(i, false)) || notes(proj.pattern(i, true));
            const bool hearing = playing && p.playingPattern() == i;
            const bool queued = playing && p.queuedPattern() == i;
            drawSlot(g, r, has, hearing);
            if (queued)
            {
                g.setColour(colour::orange);
                g.drawRoundedRectangle(r.reduced(1.0f), 3.0f, 1.5f);
            }
            const bool edited = i == editor.editPattern;
            if (edited && ! hearing)
            {
                g.setColour(colour::orange);
                g.drawRoundedRectangle(r.reduced(0.75f), 3.0f, 1.5f);
            }
            g.setColour(hearing ? colour::orangeInk : edited ? palette().slotEdited : has ? palette().slotLabel : palette().slotDim);
            g.setFont(font);
            shapedText(g, two(i), r, juce::Justification::centred, false);
        }
        for (bool b : { false, true })
        {
            const auto r = variationBounds(b).toFloat();
            const bool on = editor.editB == b;
            drawCell(g, r, on ? Cell::lit : Cell::off);
            g.setColour(on ? colour::keyInk : palette().slotInk);
            g.setFont(labelFont(11.0f));
            shapedText(g, b ? "B" : "A", r, juce::Justification::centred, false);
        }
        {
            const auto r = chainLabelBounds().toFloat();
            drawLens(g, { r.getX() + 8.0f, r.getCentreY() }, proj.chainOn);
            g.setColour(proj.chainOn ? colour::caption : colour::dim);
            g.setFont(labelFont(10.5f).withExtraKerningFactor(0.1f));
            shapedText(g, "CHAIN", r.withTrimmedLeft(16.0f), juce::Justification::centredLeft, false);
        }
        const int shown = shownChain();
        for (int i = 0; i < shown; ++i)
        {
            const auto& e = proj.chain[static_cast<size_t>(i)];
            const auto r = chainBounds(i).toFloat();
            const bool cur = playing && proj.chainOn && p.chainPosition() == i;
            drawSlot(g, r, true, cur, proj.chainOn ? 1.0f : 0.6f);
            g.setColour(cur ? colour::orangeInk : palette().slotInk.withAlpha(proj.chainOn ? 1.0f : 0.7f));
            g.setFont(font);
            shapedText(g, two(e.pattern) + (e.variationB ? "B" : "") + (e.repeats > 1 ? juce::String::charToString(0xd7) + juce::String(e.repeats) : juce::String()),
                       r, juce::Justification::centred, false);
        }
        if (static_cast<int>(proj.chain.size()) > shown)
        {
            g.setColour(colour::dim);
            g.setFont(font);
            shapedText(g, "+" + juce::String(static_cast<int>(proj.chain.size()) - shown) + " more", chainBounds(shown), juce::Justification::centred, false);
        }
        drawCell(g, addBounds().toFloat(), Cell::off);
        g.setColour(palette().slotLabel);
        g.setFont(labelFont(13.0f));
        shapedText(g, "+", addBounds(), juce::Justification::centred, false);
        drawKey(g, loopBounds().toFloat().withTrimmedBottom(1.0f), "Loop", proj.loop, true, true, false, hover.area == loopBounds());
        if (! hover.area.isEmpty() && hover.area != loopBounds())
            drawHover(g, hover.area.toFloat());
    }

    void PatternRow::mouseMove(const juce::MouseEvent& e)
    {
        const auto p = e.getPosition();
        juce::Rectangle<int> at;
        for (int i = 0; i < 16 && at.isEmpty(); ++i)
            if (slotBounds(i).contains(p))
                at = slotBounds(i);
        for (const bool b : { false, true })
            if (variationBounds(b).contains(p))
                at = variationBounds(b);
        for (int i = 0; i < shownChain(); ++i)
            if (chainBounds(i).contains(p))
                at = chainBounds(i);
        for (const auto r : { chainLabelBounds(), addBounds(), loopBounds() })
            if (r.contains(p))
                at = r;
        hover.moveTo(*this, at);
    }

    juce::String PatternRow::hintAt(juce::Point<int> p) const
    {
        for (int i = 0; i < 16; ++i)
            if (slotBounds(i).contains(p))
                return "Pattern " + two(i) + ": click to edit it (while playing, it comes in next). Right-click to copy, paste, clear or rename.";
        for (const bool b : { false, true })
            if (variationBounds(b).contains(p))
                return juce::String("Variation ") + (b ? "B" : "A") + " of the pattern: each pattern has two.";
        if (chainLabelBounds().contains(p))
            return "Chain on: the chain's patterns play in turn; off, the pattern you edit loops.";
        for (int i = 0; i < shownChain(); ++i)
            if (chainBounds(i).contains(p))
                return "In the chain: click to edit this pattern, Shift-click or wheel for its repeats, right-click to take it out.";
        if (addBounds().contains(p))
            return "Add the pattern you edit to the end of the chain.";
        if (loopBounds().contains(p))
            return "Loop: the chain plays round; off, once through and then it stops.";
        return {};
    }

    void PatternRow::mouseDown(const juce::MouseEvent& e)
    {
        auto& p = editor.processor;
        const auto pos = e.getPosition();
        for (int i = 0; i < 16; ++i)
            if (slotBounds(i).contains(pos))
            {
                if (e.mods.isPopupMenu())
                    patternMenu(i);
                else
                    editor.choosePattern(i, editor.editB);
                return;
            }
        for (bool b : { false, true })
            if (variationBounds(b).contains(pos))
            {
                editor.choosePattern(editor.editPattern, b);
                return;
            }
        if (chainLabelBounds().contains(pos))
        {
            p.edit([](Project& pr) { pr.chainOn = ! pr.chainOn; });
            editor.refreshAll();
            return;
        }
        const int n = shownChain();
        for (int i = 0; i < n; ++i)
            if (chainBounds(i).contains(pos))
            {
                if (e.mods.isPopupMenu())
                {
                    if (p.project().chain.size() > 1)
                        p.edit([i](Project& pr) { pr.chain.erase(pr.chain.begin() + i); });
                }
                else if (e.mods.isShiftDown())
                    p.edit([i](Project& pr) { auto& r = pr.chain[static_cast<size_t>(i)].repeats; r = r % 8 + 1; });
                else
                {
                    const auto& ce = p.project().chain[static_cast<size_t>(i)];
                    editor.choosePattern(ce.pattern, ce.variationB);
                }
                editor.refreshAll();
                return;
            }
        if (addBounds().contains(pos) && p.project().chain.size() < PlayBank::maxChain)
        {
            p.edit([this](Project& pr) { pr.chain.push_back({ editor.editPattern, editor.editB, 1 }); });
            editor.refreshAll();
            return;
        }
        if (loopBounds().contains(pos))
        {
            p.edit([](Project& pr) { pr.loop = ! pr.loop; });
            editor.refreshAll();
        }
    }

    void PatternRow::patternMenu(int index)
    {
        auto& p = editor.processor;
        const bool b = editor.editB;
        const juce::String here = two(index) + (b ? "B" : "A");
        juce::PopupMenu m;
        m.addSectionHeader("Pattern " + here);
        m.addItem("Copy", [this, index, b] { editor.clipboard = editor.processor.project().pattern(index, b); });
        m.addItem("Paste", editor.clipboard.has_value(), false, [this, index, b] {
            const auto copied = *editor.clipboard;
            editor.processor.edit([&](Project& pr) { pr.pattern(index, b) = copied; });
            editor.refreshAll();
        });
        m.addItem("Clear", [this, index, b] {
            editor.processor.edit([&](Project& pr) {
                auto& pat = pr.pattern(index, b);
                pat = Pattern {};
                for (auto& s : pat.step)
                    s.gate = Gate::rest;
            });
            editor.refreshAll();
        });
        m.addItem(b ? "Copy B to A" : "Copy A to B", [this, index, b] {
            editor.processor.edit([&](Project& pr) { pr.pattern(index, ! b) = pr.pattern(index, b); });
            editor.refreshAll();
        });
        m.addItem("Rename...", [this, index, b, &p = p] {
            auto* w = new juce::AlertWindow("Rename the pattern", {}, juce::MessageBoxIconType::NoIcon);
            w->addTextEditor("name", juce::String(p.project().pattern(index, b).name));
            w->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
            w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
            w->enterModalState(true, juce::ModalCallbackFunction::create([this, w, index, b](int result) {
                if (result == 1)
                {
                    const auto name = w->getTextEditorContents("name").trim().substring(0, 40).toStdString();
                    editor.processor.edit([&](Project& pr) { pr.pattern(index, b).name = name; });
                    editor.refreshAll();
                }
            }), true);
        });
        juce::ignoreUnused(p);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this));
    }

    void PatternRow::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
    {
        auto& p = editor.processor;
        const int n = shownChain();
        for (int i = 0; i < n; ++i)
            if (chainBounds(i).contains(e.getPosition()))
            {
                const int d = w.deltaY > 0.0f ? 1 : -1;
                p.edit([i, d](Project& pr) { auto& r = pr.chain[static_cast<size_t>(i)].repeats; r = juce::jlimit(1, 16, r + d); });
                repaint();
                return;
            }
    }

    // ---------------------------------------------------------------- the tool row
    ToolRow::ToolRow(Editor& e) : editor(e)
    {
        auto& p = editor.processor;
        labels.push_back({ "NEW LINE", { 12, 6, 126, 16 } });
        button("Generate", { 12, 24, 126, 28 }, true).onClick = [this] {
            const auto seed = editor.nextSeed++ * 2654435761u;
            const auto& proj = editor.processor.project();
            const auto scale = proj.scale;
            const int root = proj.root;
            editor.editPatternWith([=](Pattern& pat) { generate(pat, seed, scale, root); });
        };
        button("Mutate", { 12, 56, 94, 28 }).onClick = [this] {
            const auto seed = editor.nextSeed++ * 2246822519u;
            const auto& proj = editor.processor.project();
            const auto scale = proj.scale;
            const int root = proj.root;
            const auto what = editor.mutateWhat;
            editor.editPatternWith([=](Pattern& pat) { mutate(pat, seed, scale, root, what); });
        };
        auto& what = button(juce::String::charToString(0x25be), { 110, 56, 28, 28 });
        what.setHelpText("What Mutate may change: notes, accents, slides, rests and ties.");
        what.onClick = [this] {
            juce::PopupMenu m;
            m.addSectionHeader("Mutate may change");
            auto& w = editor.mutateWhat;
            m.addItem("Notes", true, w.notes, [this] { editor.mutateWhat.notes = ! editor.mutateWhat.notes; });
            m.addItem("Accents", true, w.accents, [this] { editor.mutateWhat.accents = ! editor.mutateWhat.accents; });
            m.addItem("Slides", true, w.slides, [this] { editor.mutateWhat.slides = ! editor.mutateWhat.slides; });
            m.addItem("Rests and ties", true, w.rests, [this] { editor.mutateWhat.rests = ! editor.mutateWhat.rests; });
            m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this));
        };
        labels.push_back({ "KEY", { 12, 104, 126, 16 } });
        auto& root = box({ 12, 122, 34, 24 });
        root.setHelpText("The key's root, for Generate, Mutate and Snap to key. Click, right-click or wheel.");
        root.text = [&p = p] { return juce::String(letters[p.project().root]); };
        root.onClick = [&p = p] { p.edit([](Project& pr) { pr.root = (pr.root + 1) % 12; }, false); };
        root.onRightClick = [&p = p] { p.edit([](Project& pr) { pr.root = (pr.root + 11) % 12; }, false); };
        root.onWheel = [&p = p](int d) { p.edit([d](Project& pr) { pr.root = (pr.root + 12 + d) % 12; }, false); };
        auto& scale = box({ 50, 122, 88, 24 });
        scale.setHelpText("The key's scale. Click, right-click or wheel.");
        scale.text = [&p = p] { return juce::String(scaleName(p.project().scale)); };
        const auto nextScale = [&p = p](int d) {
            p.edit([d](Project& pr) { pr.scale = static_cast<Scale>((static_cast<int>(pr.scale) + d + static_cast<int>(Scale::count)) % static_cast<int>(Scale::count)); }, false);
        };
        scale.onClick = [nextScale] { nextScale(1); };
        scale.onRightClick = [nextScale] { nextScale(-1); };
        scale.onWheel = nextScale;
        button("Snap to key", { 12, 152, 126, 28 }).onClick = [this] {
            const auto& proj = editor.processor.project();
            const auto sc = proj.scale;
            const int r = proj.root;
            editor.editPatternWith([=](Pattern& pat) { snapToKey(pat, sc, r); });
        };
        labels.push_back({ "MOVE", { 12, 200, 50, 28 } });
        button(juce::String::charToString(0x25c0), { 66, 200, 34, 28 }).setHelpText("Move the whole pattern a step earlier.");
        static_cast<Key&>(*parts.back()).onClick = [this] { editor.editPatternWith([](Pattern& pat) { shift(pat, -1); }); };
        button(juce::String::charToString(0x25b6), { 104, 200, 34, 28 }).setHelpText("Move the whole pattern a step later.");
        static_cast<Key&>(*parts.back()).onClick = [this] { editor.editPatternWith([](Pattern& pat) { shift(pat, 1); }); };
        labels.push_back({ "PITCH", { 12, 236, 50, 28 } });
        button(juce::String::charToString(0x2212), { 66, 236, 34, 28 }).setHelpText("Every note of the pattern a semitone down.");
        static_cast<Key&>(*parts.back()).onClick = [this] { editor.editPatternWith([](Pattern& pat) { semitone(pat, -1); }); };
        button("+", { 104, 236, 34, 28 }).setHelpText("Every note of the pattern a semitone up.");
        static_cast<Key&>(*parts.back()).onClick = [this] { editor.editPatternWith([](Pattern& pat) { semitone(pat, 1); }); };
    }

    Key& ToolRow::button(const juce::String& text, juce::Rectangle<int> place, bool hot)
    {
        auto k = std::make_unique<Key>(text, false);
        k->hot = hot; // Generate: the column's one orange action
        k->setBounds(place);
        addAndMakeVisible(*k);
        auto& ref = *k;
        parts.push_back(std::move(k));
        return ref;
    }

    ValueBox& ToolRow::box(juce::Rectangle<int> place)
    {
        auto b = std::make_unique<ValueBox>();
        b->setBounds(place);
        addAndMakeVisible(*b);
        auto& ref = *b;
        parts.push_back(std::move(b));
        return ref;
    }

    void ToolRow::paint(juce::Graphics& g)
    {
        g.setFont(labelFont(10.5f).withExtraKerningFactor(0.1f));
        g.setColour(colour::screenDim);
        for (const auto& [name, place] : labels)
            shapedText(g, name, place, juce::Justification::centredLeft, false);
    }

    void ToolRow::refresh()
    {
        repaint();
        for (auto& c : parts)
            c->repaint();
    }

    // ---------------------------------------------------------------- the step grid
    StepGrid::StepGrid(Editor& e) : editor(e) {}

    juce::Rectangle<float> StepGrid::rowBounds(Row row) const
    {
        const float w = static_cast<float>(getWidth());
        switch (row)
        {
            case Row::lights: return { 0.0f, 0.0f, w, 14.0f };
            case Row::names: return { 0.0f, 19.0f, w, 12.0f };
            case Row::pitch: return { 0.0f, 33.0f, w, 136.0f };
            case Row::octave: return { 0.0f, 174.0f, w, 20.0f };
            case Row::accent: return { 0.0f, 199.0f, w, 20.0f };
            case Row::slide: return { 0.0f, 224.0f, w, 20.0f };
            case Row::gate: return { 0.0f, 249.0f, w, 20.0f };
            case Row::motion: return { 0.0f, 274.0f, w, 36.0f };
            case Row::none: break;
        }
        return {};
    }

    juce::Rectangle<float> StepGrid::cell(Row row, int step) const
    {
        const float x0 = static_cast<float>(labelWidth);
        const float cw = (static_cast<float>(getWidth()) - x0 - 15.0f * 4.0f - 3.0f * 3.0f) / 16.0f;
        const float x = x0 + static_cast<float>(step) * (cw + 4.0f) + static_cast<float>(step / 4) * 3.0f;
        const auto r = rowBounds(row);
        return { x, r.getY(), cw, r.getHeight() };
    }

    int StepGrid::columnAt(float x) const
    {
        for (int i = 0; i < 16; ++i)
        {
            const auto c = cell(Row::lights, i);
            if (x >= c.getX() - 2.0f && x < c.getRight() + 2.0f)
                return i;
        }
        return -1;
    }

    StepGrid::Row StepGrid::rowAt(float y) const
    {
        for (auto r : { Row::lights, Row::names, Row::pitch, Row::octave, Row::accent, Row::slide, Row::gate, Row::motion })
        {
            const auto b = rowBounds(r);
            if (y >= b.getY() - 2.0f && y < b.getBottom() + 2.0f)
                return r;
        }
        return Row::none;
    }

    void StepGrid::paint(juce::Graphics& g)
    {
        auto& p = editor.processor;
        const auto& pat = editor.edited();
        const bool same = p.isPlaying() && p.playingPattern() == editor.editPattern && p.playingVariationB() == editor.editB;
        const int head = same ? p.playingStep() : -1;

        // the grid under its dark screen, the notes' names down its edge included
        drawScreen(g, juce::Rectangle<float>(static_cast<float>(labelWidth) - 40.0f, 0.0f, static_cast<float>(getWidth() - labelWidth) + 40.0f, static_cast<float>(getHeight())));
        // the labels
        g.setFont(labelFont(11.0f).withExtraKerningFactor(0.08f));
        g.setColour(colour::rowName);
        for (auto [row, name] : { std::pair { Row::octave, "OCTAVE" }, std::pair { Row::accent, "ACCENT" }, std::pair { Row::slide, "SLIDE" },
                                  std::pair { Row::gate, "GATE" } })
            shapedText(g, name, rowBounds(row).withWidth(100.0f).translated(4.0f, 0.0f), juce::Justification::centredLeft, false);
        {
            const auto r = rowBounds(Row::pitch);
            shapedText(g, "PITCH", juce::Rectangle<float>(4.0f, r.getCentreY() - 24.0f, 70.0f, 14.0f), juce::Justification::centredLeft, false);
            g.setColour(colour::hint);
            g.setFont(bodyFont(10.5f));
            shapedText(g, "Drag up or", juce::Rectangle<float>(4.0f, r.getCentreY() - 6.0f, 64.0f, 13.0f), juce::Justification::centredLeft, false);
            shapedText(g, "down, or", juce::Rectangle<float>(4.0f, r.getCentreY() + 7.0f, 64.0f, 13.0f), juce::Justification::centredLeft, false);
            shapedText(g, "wheel", juce::Rectangle<float>(4.0f, r.getCentreY() + 20.0f, 64.0f, 13.0f), juce::Justification::centredLeft, false);
            // the notes down the lane's edge, as a piano roll's keys; the picked step's note lit
            const juce::Rectangle<float> strip(static_cast<float>(labelWidth) - 36.0f, r.getY(), 30.0f, r.getHeight());
            const auto& ps = pat.step[static_cast<size_t>(editor.pick)];
            for (int n = 0; n <= 12; ++n)
            {
                const float rh = strip.getHeight() / 13.0f;
                const juce::Rectangle<float> rr(strip.getX(), strip.getBottom() - (n + 1) * rh, strip.getWidth(), rh);
                const bool sharp = n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
                const bool lit = ps.gate != Gate::rest && ps.note == n;
                g.setColour(lit ? colour::orange : sharp ? juce::Colour(0xff1b1c1f) : juce::Colour(0xffc9c8c3));
                g.fillRect(rr);
                g.setColour(juce::Colours::black.withAlpha(0.25f));
                g.drawHorizontalLine(juce::roundToInt(rr.getBottom()) - 1, rr.getX(), rr.getRight());
                g.setColour(lit ? colour::orangeInk : sharp ? juce::Colour(0xffa9abaf) : juce::Colour(0xff2a2b2e));
                g.setFont(monoFont(8.0f));
                shapedText(g, juce::String(letters[n]) + (n == 12 ? juce::String::charToString(0x2032) : juce::String()), rr.withTrimmedRight(3.0f),
                           juce::Justification::centredRight, false);
            }
            g.setColour(juce::Colour(0xff111111));
            g.drawRect(strip.expanded(1.0f), 1.0f);
        }
        {
            const auto r = rowBounds(Row::motion);
            g.setColour(colour::rowName);
            g.setFont(labelFont(11.0f).withExtraKerningFactor(0.08f));
            shapedText(g, "MOTION", juce::Rectangle<float>(4.0f, r.getY() - 2.0f, 80.0f, 14.0f), juce::Justification::centredLeft, false);
            // the knob's well sits on the grid's foot, its bottom level with the screen's (the user)
            drawWell(g, juce::Rectangle<float>(4.0f, static_cast<float>(getHeight()) - 20.0f, 62.0f, 20.0f), knobName(editor.motionKnob), colour::amber);
        }

        const auto clip = g.getClipBounds().toFloat();
        for (int i = 0; i < 16; ++i)
        {
            // only the columns the repaint reaches (a press or the beat repaints one or two)
            const auto column = cell(Row::lights, i).withTop(0.0f).withBottom(static_cast<float>(getHeight())).expanded(8.0f, 0.0f);
            if (! clip.intersects(column))
                continue;
            const auto& s = pat.step[static_cast<size_t>(i)];
            const bool off = i >= pat.length;
            const float alpha = off ? 0.35f : 1.0f;
            // the light and the step's number
            {
                const auto c = cell(Row::lights, i);
                const bool lit = i == head;
                drawStepLamp(g, { c.getCentreX() - 8.0f, c.getCentreY() }, lit); // red, as the 808's (the user)
                g.setColour(lit ? juce::Colour(0xfff4efe4) : palette().gridNumber);
                g.setFont(monoFont(9.5f));
                shapedText(g, juce::String(i + 1), c.withTrimmedLeft(c.getWidth() * 0.5f - 3.0f), juce::Justification::centredLeft, false);
            }
            // the note's name and octave
            {
                const auto c = cell(Row::names, i);
                juce::String t = s.gate == Gate::rest ? juce::String::charToString(0xb7) : juce::String(letters[s.note]) + (s.note == 12 ? juce::String::charToString(0x2032) : juce::String());
                if (s.gate == Gate::tie)
                    t = "tie";
                else if (s.gate != Gate::rest && s.octave != 0)
                    t << (s.octave > 0 ? "+" : juce::String::charToString(0x2212));
                g.setColour(juce::Colour(0xffc9c4b8).withAlpha(alpha));
                g.setFont(monoFont(10.5f));
                shapedText(g, t, c, juce::Justification::centred, false);
            }
            // the pitch lane
            {
                const auto c = cell(Row::pitch, i);
                const float rh = c.getHeight() / 13.0f;
                for (int n = 0; n < 13; ++n)
                {
                    g.setColour(juce::Colours::white.withAlpha(n % 2 == 0 ? palette().laneHi : palette().laneLo));
                    g.fillRect(juce::Rectangle<float>(c.getX(), c.getBottom() - (n + 1) * rh, c.getWidth(), rh));
                }
                // a tie draws the note it holds: the last note before it, through any ties (none after a rest)
                int shownNote = s.note;
                bool shown = s.gate == Gate::note;
                bool heldAccent = s.accent;
                if (s.gate == Gate::tie)
                    for (int j = i - 1; j >= 0; --j)
                    {
                        const auto& b = pat.step[static_cast<size_t>(j)];
                        if (b.gate == Gate::rest)
                            break;
                        if (b.gate == Gate::note)
                        {
                            shownNote = b.note;
                            heldAccent = b.accent;
                            shown = true;
                            break;
                        }
                    }
                if (shown)
                {
                    const float y = c.getBottom() - (shownNote + 1) * rh - 1.0f;
                    auto capR = juce::Rectangle<float>(c.getX() + 3.0f, y, c.getWidth() - 6.0f, rh + 2.0f);
                    // a note and the ties that hold it read as one long bar (D354): the note runs on to its column's
                    // edge when a tie follows, the tie reaches back over the gap to meet it
                    const bool tie = s.gate == Gate::tie;
                    const bool tiedOn = i + 1 < pat.length && i + 1 < 16 && pat.step[static_cast<size_t>(i + 1)].gate == Gate::tie;
                    if (tie)
                        capR = capR.withLeft(c.getX() - 8.0f);
                    if (tiedOn)
                        capR = capR.withRight(c.getRight() + 2.0f);
                    const bool playingHere = i == head;
                    // lit from inside: an accent near white; a tie the held note's own colour, a little darker
                    const bool accented = tie ? heldAccent : s.accent;
                    const auto lit = playingHere ? colour::light : accented ? colour::accentNote : colour::orange;
                    const float held = tie ? 0.78f : 1.0f;
                    glowRounded(g, capR, 2.0f, lit, 3.0f, 0.3f * alpha * held);
                    const auto upper = (playingHere ? juce::Colours::white : accented ? colour::light : palette().hotTop).withMultipliedBrightness(held);
                    const auto bottom = lit.withMultipliedBrightness(held);
                    g.setGradientFill(juce::ColourGradient(upper.withMultipliedAlpha(alpha), 0.0f, capR.getY(), bottom.withMultipliedAlpha(alpha), 0.0f, capR.getBottom(), false));
                    g.fillRoundedRectangle(capR, 2.0f);
                    if (s.slide)
                    {
                        g.setColour(palette().lensLit.withAlpha(alpha));
                        g.fillRect(juce::Rectangle<float>(c.getRight() - 3.0f, y + rh * 0.5f, 10.0f, 2.0f));
                    }
                }
                if (s.hasLocks())
                {
                    g.setColour(colour::amber);
                    g.fillEllipse(juce::Rectangle<float>(5.0f, 5.0f).withCentre({ c.getRight() - 5.5f, c.getBottom() - 5.5f }));
                }
                if (i == editor.pick)
                {
                    g.setColour(juce::Colours::white.withAlpha(0.06f));
                    g.drawRoundedRectangle(c.expanded(2.5f), 4.0f, 3.0f);
                    g.setColour(juce::Colours::white.withAlpha(0.75f));
                    g.drawRoundedRectangle(c.expanded(1.0f), 4.0f, 1.5f);
                }
                if (off)
                {
                    g.setColour(palette().screenTop.withAlpha(0.65f));
                    g.fillRect(c);
                }
            }
            // the rows of keys
            {
                const auto o = cell(Row::octave, i);
                // a tie or a rest plays no octave of its own: its octave shown half lit
                const bool own = s.gate == Gate::note;
                drawCell(g, o, s.octave == 0 ? Cell::off : own ? Cell::lit : Cell::half, alpha);
                if (s.octave != 0)
                {
                    g.setColour((own ? colour::keyInk : palette().gridHalf).withAlpha(alpha));
                    g.setFont(monoFont(11.0f));
                    shapedText(g, s.octave > 0 ? "Up" : "Down", o, juce::Justification::centred, false);
                }
                drawCell(g, cell(Row::accent, i), s.accent ? Cell::hot : Cell::off, alpha);
                const auto sl = cell(Row::slide, i);
                drawCell(g, sl, s.slide ? Cell::lit : Cell::off, alpha);
                drawLens(g, sl.getCentre(), s.slide);
                const auto gt = cell(Row::gate, i);
                drawCell(g, gt, own ? Cell::lit : Cell::off, alpha);
                g.setColour((own ? colour::keyInk : palette().gridOff).withAlpha(alpha));
                g.setFont(monoFont(11.0f));
                shapedText(g, s.gate == Gate::note ? "Note" : s.gate == Gate::tie ? "Tie" : "Rest", gt, juce::Justification::centred, false);
            }
            // the motion lane
            {
                const auto m = cell(Row::motion, i);
                g.setColour(juce::Colours::white.withAlpha(palette().motionFill));
                g.fillRoundedRectangle(m, 3.0f);
                const float v = pat.motion[static_cast<size_t>(editor.motionKnob)][static_cast<size_t>(i)];
                if (v >= 0.0f)
                {
                    const auto bar = m.reduced(2.0f).withTop(m.getY() + 2.0f + (1.0f - v) * (m.getHeight() - 4.0f));
                    glowRounded(g, bar.withHeight(std::max(3.0f, bar.getHeight())), 2.0f, colour::orange, 3.0f, 0.25f);
                    g.setGradientFill(juce::ColourGradient(i == head ? colour::light : colour::accentNote, bar.getX(), bar.getY(),
                                                           i == head ? palette().lensLit : colour::orange, bar.getX(), bar.getBottom(), false));
                    g.fillRoundedRectangle(bar.withHeight(std::max(3.0f, bar.getHeight())), 2.0f);
                }
            }
        }
        if (! hover.area.isEmpty())
            drawHover(g, hover.area.toFloat());
    }

    void StepGrid::mouseMove(const juce::MouseEvent& e)
    {
        const auto row = rowAt(e.position.y);
        const int i = e.position.x >= static_cast<float>(labelWidth) ? columnAt(e.position.x) : -1;
        juce::Rectangle<float> at;
        if (i >= 0 && row != Row::none && row != Row::lights && row != Row::names)
        {
            at = cell(row, i);
            if (row == Row::pitch)
            {
                // the pitch lane: only the note's slot a click would set, not the whole column
                const float rh = at.getHeight() / 13.0f;
                const int n = juce::jlimit(0, 12, static_cast<int>((at.getBottom() - e.position.y) / rh));
                at = juce::Rectangle<float>(at.getX(), at.getBottom() - static_cast<float>(n + 1) * rh, at.getWidth(), rh);
            }
        }
        hover.moveTo(*this, at.toNearestInt());
    }

    juce::String StepGrid::hintAt(juce::Point<int> p) const
    {
        const auto pos = p.toFloat();
        const auto row = rowAt(pos.y);
        if (pos.x < static_cast<float>(labelWidth))
            return row == Row::motion ? "Which knob's recorded moves the lane below shows: click for the next, right-click for the one before." : juce::String();
        const int i = columnAt(pos.x);
        if (i < 0)
            return {};
        const auto n = "Step " + juce::String(i + 1);
        switch (row)
        {
            case Row::lights:
            case Row::names: return n + ": its note. The lamp lights while it plays.";
            case Row::pitch: return n + "'s note: drag up or down, or wheel. Click picks the step; right-click for more.";
            case Row::octave: return n + "'s octave: click for up, down or as written.";
            case Row::accent: return n + "'s accent: louder, brighter, a shorter decay. Click to switch.";
            case Row::slide: return n + "'s slide into the next step. Click to switch.";
            case Row::gate: return n + "'s gate: a note, a tie (the note before holds on) or a rest. Click to change.";
            case Row::motion: return "The " + juce::String(knobName(editor.motionKnob)).toLowerCase() + "'s recorded move on " + n.toLowerCase() + ": draw it; right-click to clear.";
            case Row::none: break;
        }
        return {};
    }

    void StepGrid::repaintStep(int step)
    {
        if (step < 0 || step >= 16)
            return;
        repaint(cell(Row::lights, step).withTop(0.0f).withBottom(static_cast<float>(getHeight())).expanded(10.0f, 0.0f).toNearestInt());
    }

    void StepGrid::setPitchFromY(int step, float y)
    {
        const auto c = cell(Row::pitch, step);
        const int n = juce::jlimit(0, 12, static_cast<int>((1.0f - (y - c.getY()) / c.getHeight()) * 13.0f));
        editor.editPatternWith([step, n](Pattern& pat) {
            auto& s = pat.step[static_cast<size_t>(step)];
            s.note = n;
            if (s.gate == Gate::rest)
                s.gate = Gate::note;
        });
    }

    void StepGrid::setMotionAt(juce::Point<float> p)
    {
        const int i = columnAt(p.x);
        if (i < 0)
            return;
        const auto m = cell(Row::motion, i);
        const float v = juce::jlimit(0.0f, 1.0f, 1.0f - (p.y - m.getY() - 2.0f) / (m.getHeight() - 4.0f));
        const auto k = static_cast<size_t>(editor.motionKnob);
        editor.editPatternWith([i, v, k](Pattern& pat) { pat.motion[k][static_cast<size_t>(i)] = v; });
    }

    void StepGrid::stepMenu(int step)
    {
        juce::PopupMenu m;
        m.addSectionHeader("Step " + juce::String(step + 1));
        m.addItem("Copy to every 4th step", [this, step] {
            editor.editPatternWith([step](Pattern& pat) {
                const auto s = pat.step[static_cast<size_t>(step)];
                for (int j = step % 4; j < Pattern::steps; j += 4)
                    pat.step[static_cast<size_t>(j)] = s;
            });
        });
        m.addItem("Clear its locks", [this, step] { editor.editPatternWith([step](Pattern& pat) { pat.step[static_cast<size_t>(step)].lock.fill(none); }); });
        m.addItem("Rest", [this, step] { editor.editPatternWith([step](Pattern& pat) { pat.step[static_cast<size_t>(step)].gate = Gate::rest; }); });
        m.addItem("The pattern ends here (" + juce::String(step + 1) + " steps)", [this, step] { editor.editPatternWith([step](Pattern& pat) { pat.length = step + 1; }); });
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this));
    }

    void StepGrid::mouseDown(const juce::MouseEvent& e)
    {
        dragRow = Row::none;
        const auto pos = e.position;
        if (pos.x < labelWidth)
        {
            if (rowAt(pos.y) == Row::motion)
            {
                const int k = (static_cast<int>(editor.motionKnob) + (e.mods.isPopupMenu() ? knobCount - 1 : 1)) % knobCount;
                editor.motionKnob = static_cast<Knob_t>(k);
                repaint();
            }
            return;
        }
        const int i = columnAt(pos.x);
        const auto row = rowAt(pos.y);
        if (i < 0 || row == Row::none)
            return;
        if (row != Row::motion)
            editor.pick = i;
        const auto at = [this, i](const std::function<void(Step&)>& f) { editor.editPatternWith([i, f](Pattern& pat) { f(pat.step[static_cast<size_t>(i)]); }); };
        switch (row)
        {
            case Row::lights:
            case Row::names:
                break;
            case Row::pitch:
                if (e.mods.isPopupMenu())
                {
                    stepMenu(i);
                    break;
                }
                editor.processor.beginGesture();
                dragRow = row;
                dragColumn = i;
                setPitchFromY(i, pos.y);
                break;
            case Row::octave: at([](Step& s) { s.octave = s.octave == 0 ? 1 : s.octave == 1 ? -1 : 0; }); break;
            case Row::accent: at([](Step& s) { s.accent = ! s.accent; }); break;
            case Row::slide: at([](Step& s) { s.slide = ! s.slide; }); break;
            case Row::gate: at([](Step& s) { s.gate = s.gate == Gate::note ? Gate::tie : s.gate == Gate::tie ? Gate::rest : Gate::note; }); break;
            case Row::motion:
                if (e.mods.isPopupMenu())
                {
                    const auto k = static_cast<size_t>(editor.motionKnob);
                    editor.editPatternWith([i, k](Pattern& pat) { pat.motion[k][static_cast<size_t>(i)] = none; });
                    break;
                }
                editor.processor.beginGesture();
                dragRow = row;
                setMotionAt(pos);
                break;
            case Row::none: break;
        }
        editor.refreshAll();
    }

    void StepGrid::mouseDrag(const juce::MouseEvent& e)
    {
        if (dragRow == Row::pitch && dragColumn >= 0)
            setPitchFromY(dragColumn, e.position.y);
        else if (dragRow == Row::motion)
            setMotionAt(e.position);
        else
            return;
        editor.refreshAll();
    }

    void StepGrid::mouseUp(const juce::MouseEvent&)
    {
        if (dragRow != Row::none)
            editor.processor.endGesture();
        dragRow = Row::none;
        dragColumn = -1;
    }

    void StepGrid::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
    {
        const int i = columnAt(e.position.x);
        if (i < 0 || rowAt(e.position.y) != Row::pitch)
            return;
        const int d = w.deltaY > 0.0f ? 1 : -1;
        editor.pick = i;
        editor.editPatternWith([i, d](Pattern& pat) {
            auto& s = pat.step[static_cast<size_t>(i)];
            s.note = juce::jlimit(0, 12, s.note + d);
        });
        editor.refreshAll();
    }

    // ---------------------------------------------------------------- the locks
    LocksStrip::LocksStrip(Editor& e) : editor(e)
    {
        auto& p = editor.processor;
        const std::pair<Knob_t, const char*> lockable[] = { { Knob_t::cutoff, "Cutoff" }, { Knob_t::envMod, "Env mod" }, { Knob_t::decay, "Decay" }, { Knob_t::accent, "Accent" } };
        for (int j = 0; j < 4; ++j)
        {
            const auto [which, name] = lockable[j];
            const auto i = static_cast<size_t>(which);
            auto k = std::make_unique<Knob>(name, 28.0f);
            k->setHelpText("Hold " + juce::String(name).toLowerCase() + " at this value on the picked step only; the panel's knob sets the other steps." + juce::String(knobHelp));
            k->showRange = false;
            k->onScreen = true;
            k->accentRim = which == Knob_t::accent;
            k->get = [this, &p = p, i] {
                const float lock = editor.edited().step[static_cast<size_t>(editor.pick)].lock[i];
                const auto& pn = p.project().panel;
                const float panel[] = { pn.cutoff, pn.resonance, pn.envMod, pn.decay, pn.accent };
                return lock >= 0.0f ? lock : panel[i];
            };
            k->dimmed = [this, i] { return editor.edited().step[static_cast<size_t>(editor.pick)].lock[i] < 0.0f; };
            k->set = [this, i](float v) {
                const int step = editor.pick;
                editor.editPatternWith([step, i, v](Pattern& pat) { pat.step[static_cast<size_t>(step)].lock[i] = v; });
                editor.refreshAll();
            };
            k->gestureStart = [&p = p] { p.beginGesture(); };
            k->gestureEnd = [&p = p] { p.endGesture(); };
            const auto b = k->preferredBounds(0, 0);
            k->setBounds((j % 2) * 66 + 8, 34 + (j / 2) * 62, b.getWidth(), b.getHeight());
            addAndMakeVisible(*k);
            parts.push_back(std::move(k));
        }
        auto clear = std::make_unique<Key>("Clear", false);
        clear->onClick = [this] {
            const int step = editor.pick;
            editor.editPatternWith([step](Pattern& pat) { pat.step[static_cast<size_t>(step)].lock.fill(none); });
            editor.refreshAll();
        };
        clear->setBounds(14, 166, 122, 26);
        addAndMakeVisible(*clear);
        parts.push_back(std::move(clear));
    }

    void LocksStrip::paint(juce::Graphics& g)
    {
        g.setColour(colour::screenCaption);
        g.setFont(labelFont(11.0f).withExtraKerningFactor(0.08f));
        shapedText(g, "STEP " + juce::String(editor.pick + 1), 14, 6, 130, 16, juce::Justification::centredLeft, false);
        g.setColour(colour::screenHint);
        g.setFont(bodyFont(10.5f));
        g.drawFittedText("Turn a knob to hold its value on this step.", juce::Rectangle<int>(14, 200, 124, 60), juce::Justification::topLeft, 4);
    }

    void LocksStrip::refresh()
    {
        repaint();
        for (auto& c : parts)
            c->repaint();
    }

    // ---------------------------------------------------------------- the section
    // ---------------------------------------------------------------- the inspector column
    InspectorColumn::InspectorColumn(Editor& e) : editor(e)
    {
        locks = std::make_unique<LocksStrip>(editor);
        tools = std::make_unique<ToolRow>(editor);
        for (juce::Component* c : { static_cast<juce::Component*>(locks.get()), static_cast<juce::Component*>(tools.get()) })
        {
            c->setBounds(0, 36, 150, 276);
            addChildComponent(*c);
        }
        showLine(true);
    }

    void InspectorColumn::showLine(bool l)
    {
        line = l;
        tools->setVisible(line);
        locks->setVisible(! line);
        repaint();
    }

    void InspectorColumn::paint(juce::Graphics& g)
    {
        drawScreen(g, getLocalBounds().toFloat());
        drawScreen(g, juce::Rectangle<float>(4.0f, 4.0f, 142.0f, 30.0f), 5.0f);
        for (const bool l : { false, true })
        {
            const auto r = tabBounds(l).toFloat();
            drawCell(g, r, line == l ? Cell::lit : Cell::off);
            g.setColour(line == l ? colour::keyInk : palette().slotInk);
            g.setFont(labelFont(10.5f));
            shapedText(g, l ? "LINE" : "LOCKS", r, juce::Justification::centred, false);
        }
        if (! hover.area.isEmpty())
            drawHover(g, hover.area.toFloat());
    }

    void InspectorColumn::mouseMove(const juce::MouseEvent& e)
    {
        juce::Rectangle<int> at;
        for (const bool l : { false, true })
            if (tabBounds(l).contains(e.getPosition()))
                at = tabBounds(l);
        hover.moveTo(*this, at);
    }

    juce::String InspectorColumn::hintAt(juce::Point<int> p) const
    {
        if (tabBounds(false).contains(p))
            return "Locks: knob settings that hold on the picked step only.";
        if (tabBounds(true).contains(p))
            return "Line: tools that change the whole pattern.";
        return {};
    }

    void InspectorColumn::mouseDown(const juce::MouseEvent& e)
    {
        for (const bool l : { false, true })
            if (tabBounds(l).contains(e.getPosition()))
                showLine(l);
    }

    void InspectorColumn::refresh()
    {
        repaint();
        locks->refresh();
        tools->refresh();
    }

    // ---------------------------------------------------------------- the section
    SequencerSection::SequencerSection(Editor& e) : editor(e)
    {
        auto& p = editor.processor;
        patterns = std::make_unique<PatternRow>(editor);
        stepGrid = std::make_unique<StepGrid>(editor);
        column = std::make_unique<InspectorColumn>(editor);
        for (juce::Component* c : { static_cast<juce::Component*>(patterns.get()), static_cast<juce::Component*>(stepGrid.get()),
                                    static_cast<juce::Component*>(column.get()) })
            addAndMakeVisible(*c);
        // layout B (CoolSynth's D350): the pattern row, then the grid a row taller beside the inspector column
        patterns->setBounds(12, top + 19, 1186, 32);
        stepGrid->setBounds(12, top + 58, 1022, 312);
        column->setBounds(12 + 1022 + 14, top + 58, 150, 312);

        // Undo, Redo and Drag MIDI, small, at the head band's right
        auto undo = std::make_unique<Key>("Undo", false);
        undo->enabled = [&p = p] { return p.canUndo(); };
        undo->onClick = [this, &p = p] { p.undo(); editor.refreshAll(); };
        undo->setBounds(976, 1, 56, 21);
        auto redo = std::make_unique<Key>("Redo", false);
        redo->enabled = [&p = p] { return p.canRedo(); };
        redo->onClick = [this, &p = p] { p.redo(); editor.refreshAll(); };
        redo->setBounds(1036, 1, 56, 21);
        auto midi = std::make_unique<MidiKey>(editor);
        midi->setHelpText("Drag the pattern into your DAW as a MIDI file, or click to save it as one.");
        midi->setBounds(1100, 1, 98, 21);
        for (auto* k : { static_cast<juce::Component*>(undo.get()), static_cast<juce::Component*>(redo.get()), static_cast<juce::Component*>(midi.get()) })
            addAndMakeVisible(*k);
        headKeys.push_back(std::move(undo));
        headKeys.push_back(std::move(redo));
        headKeys.push_back(std::move(midi));
    }

    void SequencerSection::paint(juce::Graphics& g)
    {
        auto& p = editor.processor;
        juce::String t = "Sequencer " + juce::String::charToString(0xb7) + " editing " + two(editor.editPattern) + (editor.editB ? "B" : "");
        if (p.isPlaying())
        {
            t << " " << juce::String::charToString(0xb7) << " hearing " << two(p.playingPattern()) << (p.playingVariationB() ? "B" : "");
            if (p.queuedPattern() >= 0)
                t << " " << juce::String::charToString(0xb7) << " next " << two(p.queuedPattern());
            else if (p.project().chainOn)
                t << " " << juce::String::charToString(0xb7) << " chain";
        }
        if (p.transposition() != 0)
            t << " " << juce::String::charToString(0xb7) << " transposed " << (p.transposition() > 0 ? "+" : "") << juce::String(p.transposition());
        if (p.isRecording())
            t << " " << juce::String::charToString(0xb7) << " recording knob moves";
        drawSection(g, juce::Rectangle<int>(0, top, getWidth(), getHeight() - top).toFloat(), t, colour::face);
    }

    void SequencerSection::refresh()
    {
        repaint();
        patterns->repaint();
        stepGrid->repaint();
        column->refresh();
        for (auto& k : headKeys)
            k->repaint();
    }

    void SequencerSection::repaintPlaying(int previousStep, int step, bool patternMoved)
    {
        if (patternMoved)
        {
            repaint(0, 0, getWidth(), 22); // the head band's "hearing"
            patterns->repaint();
        }
        stepGrid->repaintStep(previousStep);
        stepGrid->repaintStep(step);
    }
}
