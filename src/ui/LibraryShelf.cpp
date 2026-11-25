#include "ui/LibraryShelf.h"

#include "ui/Theme.h"

namespace cs303::ui
{
    namespace
    {
        const juce::Colour rowLine { 0xff2a2b2f }, devilRed { 0xffd2453a };
        /** The chosen row or shelf: the dialog's ground warmed by the theme's orange. */
        juce::Colour pickFill() { return palette().dialogBottom.interpolatedWith(palette().orange, 0.12f); }
        constexpr const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C" };

        /** A line's steps as small bars by pitch (accents lighter, ties darker, slides as a link), off steps shaded. */
        void drawLineThumb(juce::Graphics& g, juce::Rectangle<float> r, const Pattern& p)
        {
            drawDisplay(g, r);
            const float cw = r.getWidth() / 16.0f;
            for (int i = 0; i < Pattern::steps; ++i)
            {
                const auto& s = p.step[static_cast<size_t>(i)];
                if (i >= p.length)
                {
                    g.setColour(palette().displayTop.withAlpha(0.75f));
                    g.fillRect(r.getX() + i * cw, r.getY(), cw, r.getHeight());
                    continue;
                }
                if (s.gate == Gate::rest)
                    continue;
                const float y = r.getBottom() - 4.0f - (static_cast<float>(s.key()) / 36.0f) * (r.getHeight() - 8.0f);
                g.setColour(s.gate == Gate::tie ? juce::Colour(0xffb65a20) : s.accent ? colour::accentNote : colour::orange);
                g.fillRoundedRectangle(r.getX() + i * cw + 1.0f, y - 1.5f, cw - 2.0f, 3.0f, 1.0f);
                if (s.slide)
                {
                    g.setColour(colour::light);
                    g.fillRect(r.getX() + (i + 1) * cw - 1.0f, y - 1.0f, 2.0f, 2.0f);
                }
            }
        }

        /** A sound as five bars: cutoff, resonance, env mod, decay, accent; red on a Devil Fish. */
        void drawSoundThumb(juce::Graphics& g, juce::Rectangle<float> r, const juce::var& v)
        {
            drawDisplay(g, r);
            const bool fish = static_cast<bool>(v["devilFish"]);
            int i = 0;
            for (const char* k : { "cutoff", "resonance", "envMod", "decay", "accent" })
            {
                const float value = v[k].isVoid() ? 0.5f : static_cast<float>(static_cast<double>(v[k]));
                const auto bar = juce::Rectangle<float>(r.getX() + 10.0f + i * 24.0f, r.getY() + 6.0f, 14.0f, r.getHeight() - 12.0f);
                g.setColour(colour::cell);
                g.fillRoundedRectangle(bar, 2.0f);
                g.setColour(fish ? devilRed : colour::orange);
                g.fillRoundedRectangle(bar.withTrimmedTop(bar.getHeight() * (1.0f - value)), 2.0f);
                ++i;
            }
        }

        void drawKnob(juce::Graphics& g, juce::Point<float> c, float value, bool red, const juce::String& name)
        {
            g.setColour(colour::well);
            g.fillEllipse(c.x - 17.0f, c.y - 17.0f, 34.0f, 34.0f);
            g.setColour(colour::wellLine);
            g.drawEllipse(c.x - 17.0f, c.y - 17.0f, 34.0f, 34.0f, 1.0f);
            const float a = juce::degreesToRadians(-135.0f + 270.0f * value);
            g.setColour(red ? devilRed : colour::orange);
            g.drawLine(c.x, c.y, c.x + 15.0f * std::sin(a), c.y - 15.0f * std::cos(a), 3.0f);
            g.setColour(colour::screenDim);
            g.setFont(labelFont(9.5f));
            shapedText(g, name, juce::Rectangle<float>(c.x - 40.0f, c.y + 20.0f, 80.0f, 14.0f), juce::Justification::centred, false);
        }

        juce::String percentText(const juce::var& v) { return juce::String(juce::roundToInt(static_cast<double>(v) * 10.0)); }
    }

    void LibraryShelf::recolour()
    {
        list.setColour(juce::ListBox::backgroundColourId, palette().dialogBottom);
        search.setTextToShowWhenEmpty("Search lines and sounds", colour::screenDim);
        search.setColour(juce::TextEditor::backgroundColourId, colour::well);
        search.setColour(juce::TextEditor::outlineColourId, colour::wellLine);
        search.setColour(juce::TextEditor::textColourId, colour::ink);
        search.applyColourToAllText(colour::ink);
        list.repaint();
        repaint();
    }

    LibraryShelf::LibraryShelf()
    {
        setWantsKeyboardFocus(true);
        list.setModel(this);
        list.setRowHeight(48);
        list.setOutlineThickness(0);
        addAndMakeVisible(list);
        search.setFont(bodyFont(13.0f));
        recolour();
        search.onTextChange = [this] { searchFor(search.getText()); };
        addAndMakeVisible(search);

        const auto make = [this](const juce::String& text) {
            auto k = std::make_unique<Key>(text, false);
            addAndMakeVisible(*k);
            return k;
        };
        close = make(juce::String::charToString(0x2715));
        close->onClick = [this] { if (onClose) onClose(); };
        audition = make("Audition");
        audition->isOn = [this] { return isAuditioning(); };
        audition->textOf = [this] { return juce::String(isAuditioning() ? "Stop" : "Audition"); };
        audition->onClick = [this] { toggleAudition(); };
        loadKey = make("Load");
        loadKey->onClick = [this] { load(); };
        keep = make("Keep a copy");
        keep->onClick = [this] { keepCopy(); };
        rename = make("Rename");
        rename->onClick = [this] { renameChosen(); };
        recycle = make("To the Recycle Bin");
        recycle->onClick = [this] { recycleChosen(); };
        close->setHelpText("Close the library (Esc). An audition still sounding stops and gives yours back.");
        audition->setHelpText("Hear the chosen line with your sound, or your pattern with the chosen sound, in place of yours. Stop gives yours back. Space does the same.");
        loadKey->setHelpText("Put the chosen line into the pattern you edit, or the chosen sound on the panel. One undo step. Enter does the same.");
        keep->setHelpText("Keep a copy of the chosen line or sound on your own shelf, to rename and change.");
        rename->setHelpText("Rename this one of yours.");
        recycle->setHelpText("Move this one of yours to the Recycle Bin.");
        search.setHelpText("Search the lines and sounds by name, style or family.");
        keepMine = make("Keep");
        keepMine->setHelpText("Keep the pattern you edit, or the panel's sound, on your own shelf.");
        keepMine->onClick = [this]
        {
            if (shelf == "mine:lines" && currentLine)
                saveUserLine(currentLine());
            else if (shelf == "mine:sounds" && currentSound)
                saveUserSound(currentSound());
            reload();
            selectRow(shownCount() - 1);
        };
        hintLine = std::make_unique<HintLine>(true);
        addAndMakeVisible(*hintLine);
        hintWatcher = std::make_unique<HintWatcher>(*this, *hintLine);
        addMouseListener(hintWatcher.get(), true);
        list.addMouseListener(this, true); // the row under the mouse, lit
        reload();
    }

    LibraryShelf::~LibraryShelf()
    {
        list.removeMouseListener(this);
        removeMouseListener(hintWatcher.get());
        list.setModel(nullptr);
    }

    void LibraryShelf::reload()
    {
        const auto* line = chosenLine();
        const auto* sound = chosenSound();
        const auto keepName = line != nullptr ? line->name : sound != nullptr ? sound->name : juce::String();
        lib = builtInLibrary();
        const auto mine = userLibrary();
        lib.lines.insert(lib.lines.end(), mine.lines.begin(), mine.lines.end());
        lib.sounds.insert(lib.sounds.end(), mine.sounds.begin(), mine.sounds.end());
        refill();
        for (int i = 0; i < shownCount(); ++i)
        {
            const auto& s = shown[static_cast<size_t>(i)];
            if ((s.line ? lib.lines[static_cast<size_t>(s.index)].name : lib.sounds[static_cast<size_t>(s.index)].name) == keepName)
            {
                selectRow(i);
                return;
            }
        }
    }

    juce::StringArray LibraryShelf::shelfKeys() const
    {
        juce::StringArray keys;
        for (const auto& r : side)
            if (! r.heading)
                keys.add(r.key);
        return keys;
    }

    void LibraryShelf::refill()
    {
        shown.clear();
        const auto q = query.toLowerCase();
        const bool searching = q.isNotEmpty();
        const auto kind = shelf.upToFirstOccurrenceOf(":", false, false), which = shelf.fromFirstOccurrenceOf(":", false, false);
        for (int i = 0; i < static_cast<int>(lib.lines.size()); ++i)
        {
            const auto& l = lib.lines[static_cast<size_t>(i)];
            const bool fits = searching ? (l.name + " " + l.style + " " + l.about).toLowerCase().contains(q)
                                        : (kind == "lines" && l.builtIn && (which == "all" || l.style == which)) || (shelf == "mine:lines" && ! l.builtIn);
            if (fits)
                shown.push_back({ true, i });
        }
        for (int i = 0; i < static_cast<int>(lib.sounds.size()); ++i)
        {
            const auto& s = lib.sounds[static_cast<size_t>(i)];
            const bool fits = searching ? (s.name + " " + s.family + " " + s.about).toLowerCase().contains(q)
                                        : (kind == "sounds" && s.builtIn && (which == "all" || s.family == which)) || (shelf == "mine:sounds" && ! s.builtIn);
            if (fits)
                shown.push_back({ false, i });
        }
        layOutSide();
        list.updateContent();
        chosen = shown.empty() ? -1 : 0;
        list.selectRow(chosen, true, true);
        updateKeys();
        repaint();
    }

    void LibraryShelf::layOutSide()
    {
        side.clear();
        const auto add = [this](const juce::String& key, const juce::String& label, int count, bool heading = false) {
            side.push_back({ key, label, count, heading, {} });
        };
        juce::StringArray styles, families;
        int builtLines = 0, builtSounds = 0, myLines = 0, mySounds = 0;
        for (const auto& l : lib.lines)
            if (l.builtIn) { styles.addIfNotAlreadyThere(l.style); ++builtLines; } else ++myLines;
        for (const auto& s : lib.sounds)
            if (s.builtIn) { families.addIfNotAlreadyThere(s.family); ++builtSounds; } else ++mySounds;
        add({}, "LINES", 0, true);
        add("lines:all", "All lines", builtLines);
        for (const auto& st : styles)
            add("lines:" + st, st, static_cast<int>(std::count_if(lib.lines.begin(), lib.lines.end(), [&](const LibraryLine& l) { return l.builtIn && l.style == st; })));
        add({}, "SOUNDS", 0, true);
        add("sounds:all", "All sounds", builtSounds);
        for (const auto& f : families)
            add("sounds:" + f, f, static_cast<int>(std::count_if(lib.sounds.begin(), lib.sounds.end(), [&](const LibrarySound& s) { return s.builtIn && s.family == f; })));
        add({}, "YOURS", 0, true);
        add("mine:lines", "My lines", myLines);
        add("mine:sounds", "My sounds", mySounds);
        const auto area = sideArea().toFloat();
        float y = area.getY() + 8.0f;
        for (auto& r : side)
        {
            const float h = r.heading ? 30.0f : 27.0f;
            r.area = { area.getX(), y, area.getWidth(), h };
            y += h;
        }
    }

    juce::Rectangle<int> LibraryShelf::sideArea() const noexcept { return { box().getX(), box().getY() + 52, 200, box().getHeight() - 52 - footHeight }; }
    juce::Rectangle<int> LibraryShelf::listArea() const noexcept { return { box().getX() + 200, box().getY() + 52, 400, box().getHeight() - 52 - footHeight }; }
    juce::Rectangle<int> LibraryShelf::detailArea() const noexcept
    {
        return { box().getX() + 600, box().getY() + 52, box().getWidth() - 600, box().getHeight() - 52 - footHeight };
    }

    void LibraryShelf::resized()
    {
        const auto b = box();
        search.setBounds(b.getX() + 230, b.getY() + 12, 280, 28);
        close->setBounds(b.getRight() - 42, b.getY() + 12, 28, 28);
        list.setBounds(listArea());
        const auto d = detailArea().reduced(22, 0);
        const int keysY = d.getY() + 344; // just under the preview (its how-to line gone, D351)
        audition->setBounds(d.getX(), keysY, 96, 28);
        loadKey->setBounds(d.getX() + 104, keysY, 150, 28);
        keep->setBounds(d.getX() + 262, keysY, 110, 28);
        rename->setBounds(d.getX() + 262, keysY, 80, 28);
        recycle->setBounds(d.getX() + 350, keysY, 140, 28);
        keepMine->setBounds(listArea().getX() + 14, listArea().getBottom() - 44, listArea().getWidth() - 28, 30);
        hintLine->setBounds(box().getX() + 16, box().getBottom() - footHeight + 2, box().getWidth() - 32, footHeight - 4);
        layOutSide();
    }

    void LibraryShelf::updateKeys()
    {
        const auto* line = chosenLine();
        const auto* sound = chosenSound();
        const bool any = line != nullptr || sound != nullptr;
        const bool mine = (line != nullptr && ! line->builtIn) || (sound != nullptr && ! sound->builtIn);
        audition->setVisible(any);
        loadKey->setVisible(any);
        keep->setVisible(any && ! mine);
        rename->setVisible(mine);
        recycle->setVisible(mine);
        keepMine->setVisible(shelf.startsWith("mine:") && query.isEmpty());
        keepMine->textOf = [this] { return shelf == "mine:lines" ? "Keep pattern " + loadTarget + " here" : juce::String("Keep the panel's sound here"); };
        loadKey->textOf = [this] { return chosenSound() != nullptr ? juce::String("Load the sound") : "Load into " + loadTarget; };
        repaint();
    }

    const LibraryLine* LibraryShelf::chosenLine() const
    {
        if (chosen < 0 || chosen >= shownCount() || ! shown[static_cast<size_t>(chosen)].line)
            return nullptr;
        return &lib.lines[static_cast<size_t>(shown[static_cast<size_t>(chosen)].index)];
    }

    const LibrarySound* LibraryShelf::chosenSound() const
    {
        if (chosen < 0 || chosen >= shownCount() || shown[static_cast<size_t>(chosen)].line)
            return nullptr;
        return &lib.sounds[static_cast<size_t>(shown[static_cast<size_t>(chosen)].index)];
    }

    void LibraryShelf::showShelf(const juce::String& key)
    {
        if (isAuditioning())
            toggleAudition();
        shelf = key;
        query.clear();
        search.setText({}, false);
        refill();
    }

    void LibraryShelf::searchFor(const juce::String& text)
    {
        query = text.trim();
        refill();
    }

    void LibraryShelf::selectRow(int row)
    {
        if (row < 0 || row >= shownCount())
            return;
        list.selectRow(row, true, true);
        selectedRowsChanged(row);
    }

    void LibraryShelf::selectedRowsChanged(int lastRowSelected)
    {
        if (lastRowSelected == chosen)
            return;
        const bool wasAuditioning = isAuditioning();
        chosen = lastRowSelected;
        updateKeys();
        // Walking the list while listening moves the audition along with it, in one step: the editor gives yours back
        // and puts the next one in its place, the transport untouched (no stop between them).
        if (wasAuditioning)
            startAudition();
        repaint();
    }

    void LibraryShelf::listBoxItemClicked(int row, const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu())
        {
            selectRow(row);
            const auto* line = chosenLine();
            const auto* sound = chosenSound();
            const bool mine = (line != nullptr && ! line->builtIn) || (sound != nullptr && ! sound->builtIn);
            juce::PopupMenu m;
            m.addItem("Audition", [this] { toggleAudition(); });
            m.addItem(sound != nullptr ? "Load the sound" : "Load into " + loadTarget, [this] { load(); });
            if (mine)
            {
                m.addItem("Rename", [this] { renameChosen(); });
                m.addItem("To the Recycle Bin", [this] { recycleChosen(); });
            }
            else
                m.addItem("Keep a copy", [this] { keepCopy(); });
            m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&list));
        }
    }

    void LibraryShelf::startAudition()
    {
        if (const auto* line = chosenLine())
        {
            if (onAuditionLine)
                onAuditionLine(line);
        }
        else if (const auto* sound = chosenSound())
        {
            if (onAuditionSound)
                onAuditionSound(sound);
        }
    }

    void LibraryShelf::toggleAudition()
    {
        if (isAuditioning())
        {
            if (onAuditionLine)
                onAuditionLine(nullptr); // the editor stops whichever is sounding and gives yours back
        }
        else
            startAudition();
        repaint();
    }

    void LibraryShelf::auditionEnded()
    {
        repaint();
        if (audition != nullptr)
            audition->repaint();
    }

    void LibraryShelf::load()
    {
        if (const auto* l = chosenLine())
        {
            if (onLoadLine) onLoadLine(*l);
        }
        else if (const auto* s = chosenSound())
        {
            if (onLoadSound) onLoadSound(*s);
        }
        repaint();
    }

    void LibraryShelf::keepCopy()
    {
        juce::File kept;
        const bool isLine = chosenLine() != nullptr;
        if (const auto* l = chosenLine())
            kept = saveUserLine(*l);
        else if (const auto* s = chosenSound())
            kept = saveUserSound(*s);
        if (kept == juce::File())
            return;
        lib = builtInLibrary();
        const auto mine = userLibrary();
        lib.lines.insert(lib.lines.end(), mine.lines.begin(), mine.lines.end());
        lib.sounds.insert(lib.sounds.end(), mine.sounds.begin(), mine.sounds.end());
        showShelf(isLine ? "mine:lines" : "mine:sounds");
        for (int i = 0; i < shownCount(); ++i)
        {
            const auto& s = shown[static_cast<size_t>(i)];
            const auto& f = s.line ? lib.lines[static_cast<size_t>(s.index)].file : lib.sounds[static_cast<size_t>(s.index)].file;
            if (f == kept)
                selectRow(i);
        }
    }

    void LibraryShelf::renameChosen()
    {
        const auto* line = chosenLine();
        const auto* sound = chosenSound();
        const auto file = line != nullptr ? line->file : sound != nullptr ? sound->file : juce::File();
        if (file == juce::File())
            return;
        auto* w = new juce::AlertWindow("Rename", "A new name:", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor("name", line != nullptr ? line->name : sound->name);
        w->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
        w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        w->enterModalState(true, juce::ModalCallbackFunction::create([this, w, file](int result)
        {
            if (result == 1)
            {
                const auto renamed = renameUserEntry(file, w->getTextEditorContents("name"));
                reload();
                for (int i = 0; i < shownCount(); ++i)
                {
                    const auto& s = shown[static_cast<size_t>(i)];
                    if ((s.line ? lib.lines[static_cast<size_t>(s.index)].file : lib.sounds[static_cast<size_t>(s.index)].file) == renamed)
                        selectRow(i);
                }
            }
        }), true);
    }

    void LibraryShelf::recycleChosen()
    {
        const auto* line = chosenLine();
        const auto* sound = chosenSound();
        const auto file = line != nullptr ? line->file : sound != nullptr ? sound->file : juce::File();
        if (file == juce::File())
            return;
        if (isAuditioning())
            toggleAudition();
        recycleUserEntry(file);
        const int was = chosen;
        reload();
        selectRow(std::min(was, shownCount() - 1));
    }

    void LibraryShelf::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected)
    {
        if (row < 0 || row >= shownCount())
            return;
        const auto& s = shown[static_cast<size_t>(row)];
        g.fillAll(selected ? pickFill() : palette().dialogBottom);
        if (row == hoverRow && ! selected)
            g.fillAll(juce::Colours::white.withAlpha(0.05f));
        if (selected)
        {
            g.setColour(colour::orange);
            g.fillRect(0, 0, 3, height);
        }
        g.setColour(rowLine);
        g.fillRect(0, height - 1, width, 1);
        const auto thumb = juce::Rectangle<float>(static_cast<float>(width - 146), (height - 30) / 2.0f, 132.0f, 30.0f);
        juce::String name, meta;
        if (s.line)
        {
            const auto& l = lib.lines[static_cast<size_t>(s.index)];
            name = l.name;
            const auto dot = " " + juce::String::charToString(0xb7) + " ";
            meta = l.style + dot + juce::String(juce::roundToInt(l.bpm)) + " BPM" + dot + juce::String(l.pattern.length) + " steps";
            drawLineThumb(g, thumb, l.pattern);
        }
        else
        {
            const auto& snd = lib.sounds[static_cast<size_t>(s.index)];
            name = snd.name;
            meta = snd.family + " " + juce::String::charToString(0xb7) + (static_cast<bool>(snd.values["square"]) ? " square" : " saw");
            drawSoundThumb(g, thumb, snd.values);
        }
        g.setColour(colour::ink);
        g.setFont(labelFont(12.5f));
        shapedText(g, name, 14, 7, width - 170, 18, juce::Justification::centredLeft, true);
        g.setColour(colour::screenDim);
        g.setFont(monoFont(10.5f));
        shapedText(g, meta, 14, 25, width - 170, 15, juce::Justification::centredLeft, true);
    }

    void LibraryShelf::paint(juce::Graphics& g)
    {
        g.fillAll(juce::Colour(0xff0a0a0c).withAlpha(0.62f)); // the veil over the panel
        const auto b = box().toFloat();
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillRoundedRectangle(b.translated(0.0f, 8.0f).expanded(6.0f), 14.0f);
        g.setColour(juce::Colour(0xff0c0c0d));
        g.fillRoundedRectangle(b.expanded(1.0f), 11.0f);
        g.setGradientFill(juce::ColourGradient(palette().dialogTop, 0.0f, b.getY(), palette().dialogBottom, 0.0f, b.getBottom(), false));
        g.fillRoundedRectangle(b, 10.0f);
        // the top: the title in a head band
        {
            juce::Graphics::ScopedSaveState state(g);
            juce::Path band;
            band.addRoundedRectangle(b.getX(), b.getY(), b.getWidth(), 52.0f, 10.0f, 10.0f, true, true, false, false);
            g.reduceClipRegion(band);
            g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.045f), 0.0f, b.getY(), juce::Colours::white.withAlpha(0.015f), 0.0f, b.getY() + 52.0f, false));
            g.fillRect(b.withHeight(52.0f));
        }
        g.setColour(juce::Colours::white.withAlpha(0.07f));
        g.fillRect(b.getX() + 10.0f, b.getY(), b.getWidth() - 20.0f, 1.0f);
        g.setFont(labelFont(13.0f).withExtraKerningFactor(0.14f));
        g.setColour(colour::screenCaption);
        shapedText(g, "COOLSYNTH", juce::Rectangle<float>(b.getX() + 16.0f, b.getY(), 90.0f, 52.0f), juce::Justification::centredLeft, false);
        g.setColour(colour::orange);
        shapedText(g, "303", juce::Rectangle<float>(b.getX() + 104.0f, b.getY(), 36.0f, 52.0f), juce::Justification::centredLeft, false);
        g.setColour(colour::screenCaption);
        shapedText(g, juce::String::charToString(0xb7) + " LIBRARY", juce::Rectangle<float>(b.getX() + 138.0f, b.getY(), 90.0f, 52.0f),
                   juce::Justification::centredLeft, false);
        // the shelves
        const auto sa = sideArea().toFloat();
        g.setGradientFill(juce::ColourGradient(palette().screenTop.brighter(0.15f), 0.0f, sa.getY(), palette().screenBottom.brighter(0.15f), 0.0f, sa.getBottom(), false));
        g.fillRect(sa.withTrimmedBottom(1.0f).withTrimmedLeft(1.0f));
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillRect(sa.getRight(), sa.getY(), 1.0f, sa.getHeight());
        g.fillRect(listArea().toFloat().getRight(), sa.getY(), 1.0f, sa.getHeight());
        g.fillRect(b.getX(), sa.getY(), b.getWidth(), 1.0f);
        for (const auto& r : side)
        {
            if (r.heading)
            {
                g.setColour(colour::screenDim);
                g.setFont(labelFont(10.0f).withExtraKerningFactor(0.16f));
                shapedText(g, r.label, r.area.withTrimmedLeft(16.0f).withTrimmedTop(10.0f), juce::Justification::centredLeft, false);
                continue;
            }
            const bool on = query.isEmpty() && r.key == shelf;
            if (on)
            {
                g.setColour(pickFill());
                g.fillRect(r.area);
                g.setColour(colour::orange);
                g.fillRect(r.area.withWidth(3.0f));
            }
            g.setColour(on ? colour::ink : colour::screenLabel);
            g.setFont(bodyFont(12.5f));
            shapedText(g, r.label, r.area.withTrimmedLeft(16.0f), juce::Justification::centredLeft, false);
            g.setColour(colour::screenDim);
            g.setFont(monoFont(10.5f));
            shapedText(g, juce::String(r.count), r.area.withTrimmedRight(14.0f), juce::Justification::centredRight, false);
        }
        if (shown.empty())
        {
            g.setColour(colour::screenHint);
            g.setFont(bodyFont(12.0f));
            g.drawFittedText(shelf.startsWith("mine:") ? "Nothing here yet: Keep a copy of a line or a sound, or keep yours from the panel with the key below."
                                                        : "Nothing found.",
                             listArea().reduced(18, 16).withHeight(60), juce::Justification::topLeft, 3);
        }
        paintDetail(g);
        if (! sideHover.area.isEmpty())
            drawHover(g, sideHover.area.toFloat().reduced(4.0f, 1.0f));
        // the foot: a line over the hint
        const auto foot = box().toFloat().removeFromBottom(static_cast<float>(footHeight));
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillRect(foot.getX() + 1.0f, foot.getY(), foot.getWidth() - 2.0f, 1.0f);
    }

    void LibraryShelf::paintDetail(juce::Graphics& g)
    {
        const auto d = detailArea().toFloat().reduced(22.0f, 0.0f);
        float y = d.getY() + 18.0f;
        const auto text = [&](const juce::String& t, juce::Font f, juce::Colour c, float h) {
            g.setColour(c);
            g.setFont(f);
            g.drawFittedText(t, juce::Rectangle<float>(d.getX(), y, d.getWidth(), h).toNearestInt(), juce::Justification::topLeft, 3);
            y += h;
        };
        if (const auto* l = chosenLine())
        {
            text(l->name, labelFont(20.0f), colour::ink, 30.0f);
            text("Style " + l->style + "   Tempo " + juce::String(juce::roundToInt(l->bpm)) + " BPM   Length " + juce::String(l->pattern.length)
                     + " steps" + (l->sound.isNotEmpty() ? "   Written for " + l->sound : juce::String()),
                 monoFont(11.0f), colour::screenDim, 24.0f);
            text(l->about, bodyFont(12.5f), colour::screenLabel, 40.0f);
            // the grid: pitch bars, the accent row and the slide row, as the sequencer draws them
            const auto grid = juce::Rectangle<float>(d.getX(), y + 4.0f, d.getWidth(), 210.0f);
            drawScreen(g, grid, 8.0f);
            if (isAuditioning())
            {
                g.setColour(colour::orange);
                g.drawRoundedRectangle(grid, 8.0f, 2.0f);
            }
            const auto pitch = grid.reduced(10.0f).withHeight(140.0f);
            const float cw = pitch.getWidth() / 16.0f;
            for (int k = 0; k <= 36; k += 12)
            {
                const float ly = pitch.getBottom() - 6.0f - (k / 36.0f) * (pitch.getHeight() - 12.0f);
                g.setColour(colour::cell);
                g.fillRect(pitch.getX(), ly, pitch.getWidth(), 1.0f);
            }
            for (int i = 0; i < Pattern::steps; ++i)
            {
                const auto& s = l->pattern.step[static_cast<size_t>(i)];
                const bool off = i >= l->pattern.length;
                const float x = pitch.getX() + i * cw;
                if (! off && s.gate != Gate::rest)
                {
                    const float ny = pitch.getBottom() - 6.0f - (s.key() / 36.0f) * (pitch.getHeight() - 12.0f);
                    const auto bar = juce::Rectangle<float>(x + 3.0f, ny - 5.0f, cw - 6.0f, 10.0f);
                    if (s.gate != Gate::tie)
                        glowRounded(g, bar, 2.0f, s.accent ? colour::accentNote : colour::orange, 3.0f, 0.3f);
                    g.setColour(s.gate == Gate::tie ? juce::Colour(0xff9c4c1b) : s.accent ? colour::accentNote : colour::orange);
                    g.fillRoundedRectangle(bar, 2.0f);
                    if (s.gate != Gate::tie)
                    {
                        g.setColour(colour::screenLabel);
                        g.setFont(monoFont(9.5f));
                        shapedText(g, juce::String(noteNames[s.note]) + (s.octave > 0 ? "+" : s.octave < 0 ? "-" : ""),
                                   juce::Rectangle<float>(x, ny - 19.0f, cw, 12.0f), juce::Justification::centred, false);
                    }
                }
                const float ry = pitch.getBottom() + 8.0f;
                drawCell(g, { x + 3.0f, ry, cw - 6.0f, 18.0f }, ! off && s.accent ? Cell::hot : Cell::off, off ? 0.4f : 1.0f);
                drawCell(g, { x + 3.0f, ry + 24.0f, cw - 6.0f, 18.0f }, ! off && s.slide ? Cell::lit : Cell::off, off ? 0.4f : 1.0f);
                if (! off)
                    drawLens(g, { x + cw / 2.0f, ry + 33.0f }, s.slide);
                if (off)
                {
                    g.setColour(palette().screenTop.withAlpha(0.7f));
                    g.fillRect(x, pitch.getY(), cw, pitch.getHeight());
                }
            }
        }
        else if (const auto* s = chosenSound())
        {
            text(s->name, labelFont(20.0f), colour::ink, 30.0f);
            text("Family " + s->family, monoFont(11.0f), colour::screenDim, 24.0f);
            text(s->about, bodyFont(12.5f), colour::screenLabel, 40.0f);
            const auto card = juce::Rectangle<float>(d.getX(), y + 4.0f, d.getWidth(), 170.0f);
            drawScreen(g, card, 8.0f);
            if (isAuditioning())
            {
                g.setColour(colour::orange);
                g.drawRoundedRectangle(card, 8.0f, 2.0f);
            }
            const auto& v = s->values;
            const bool fish = static_cast<bool>(v["devilFish"]);
            const std::pair<const char*, const char*> knobs[] = { { "cutoff", "CUTOFF" }, { "resonance", "RESONANCE" }, { "envMod", "ENV MOD" },
                                                                  { "decay", "DECAY" }, { "accent", "ACCENT" } };
            for (int i = 0; i < 5; ++i)
            {
                const auto val = v[knobs[i].first];
                drawKnob(g, { card.getX() + card.getWidth() * (i + 0.5f) / 5.0f, card.getY() + 40.0f },
                         val.isVoid() ? 0.5f : static_cast<float>(static_cast<double>(val)), fish, knobs[i].second);
            }
            juce::StringArray chips { static_cast<bool>(v["square"]) ? "Square" : "Saw",
                                      static_cast<bool>(v["driveOn"]) ? "Drive " + percentText(v["drive"]) : juce::String("Drive off"),
                                      static_cast<bool>(v["delayOn"]) ? "Delay mix " + percentText(v["mix"]) : juce::String("Delay off") };
            juce::StringArray red;
            if (fish)
            {
                for (const auto& [key, label] : std::initializer_list<std::pair<const char*, const char*>> {
                         { "overdrive", "overdrive" }, { "slideTime", "slide time" }, { "softAttack", "soft attack" }, { "normalDecay", "normal decay" },
                         { "accentDecay", "accent decay" }, { "tracking", "tracking" }, { "filterFm", "filter FM" } })
                    if (! v[key].isVoid())
                        red.add(juce::String(label) + " " + percentText(v[key]));
                if (static_cast<int>(v["muffler"]) > 0)
                    red.add(static_cast<int>(v["muffler"]) == 1 ? "muffler soft" : "muffler hard");
                if (! v["accentSweep"].isVoid() && static_cast<int>(v["accentSweep"]) == 1)
                    red.add("accent sweep: high res");
            }
            float cx = card.getX() + 12.0f, cy = card.getY() + 96.0f;
            g.setFont(monoFont(10.5f));
            const auto chip = [&](const juce::String& t, bool isRed) {
                const float w = shapedWidth(g.getCurrentFont(), t) + 20.0f;
                if (cx + w > card.getRight() - 10.0f) { cx = card.getX() + 12.0f; cy += 26.0f; }
                const auto r = juce::Rectangle<float>(cx, cy, w, 20.0f);
                g.setColour(isRed ? devilRed : colour::wellLine);
                g.drawRoundedRectangle(r, 10.0f, 1.0f);
                g.setColour(isRed ? juce::Colour(0xfff09a90) : colour::screenLabel);
                shapedText(g, t, r, juce::Justification::centred, false);
                cx += w + 6.0f;
            };
            for (const auto& c : chips) chip(c, false);
            for (const auto& c : red) chip(c, true);
        }
    }

    void LibraryShelf::mouseUp(const juce::MouseEvent& e)
    {
        // Only a click on the shelf itself: the list's own clicks reach here too (it is listened to for its hover
        // light, D353), their positions in a row's own place, and were taken for a click on the veil (D358).
        if (e.eventComponent != this)
            return;
        const auto p = e.position;
        for (const auto& r : side)
            if (! r.heading && r.area.contains(p))
            {
                showShelf(r.key);
                return;
            }
        if (! box().toFloat().contains(p) && onClose)
            onClose(); // a click on the veil closes, as the 808's
    }

    void LibraryShelf::mouseMove(const juce::MouseEvent& e)
    {
        // over the list: its row lit; over the shelves: the shelf's name
        if (e.eventComponent != this && list.isParentOf(e.eventComponent))
        {
            const auto at = e.getEventRelativeTo(&list).getPosition();
            const int row = list.getRowContainingPosition(at.x, at.y);
            if (row != hoverRow)
            {
                list.repaintRow(hoverRow);
                hoverRow = row;
                list.repaintRow(hoverRow);
            }
            return;
        }
        juce::Rectangle<int> at;
        for (const auto& r : side)
            if (! r.heading && r.area.toNearestInt().contains(e.getPosition()))
                at = r.area.toNearestInt();
        sideHover.moveTo(*this, at);
    }

    void LibraryShelf::mouseExit(const juce::MouseEvent& e)
    {
        if (e.eventComponent != this && list.isParentOf(e.eventComponent) && ! list.isMouseOver(true))
        {
            list.repaintRow(hoverRow);
            hoverRow = -1;
        }
        if (e.eventComponent == this)
            sideHover.moveTo(*this, {});
    }

    juce::String LibraryShelf::hintAt(juce::Point<int> p) const
    {
        for (const auto& r : side)
            if (! r.heading && r.area.toNearestInt().contains(p))
                return r.key.startsWith("mine:") ? "Your own " + r.label.toLowerCase() + ", kept from the panel or copied from the library."
                     : r.key.startsWith("sounds") ? "The sounds: " + r.label + "." : "The lines: " + r.label + ".";
        if (listArea().contains(p))
            return "Click to choose; Space auditions it, Enter loads it, right-click for more.";
        if (detailArea().contains(p))
            return "The chosen one in full: its steps as the grid draws them, or its sound's knobs.";
        return {};
    }

    bool LibraryShelf::keyPressed(const juce::KeyPress& k)
    {
        if (k == juce::KeyPress::escapeKey)
        {
            if (onClose) onClose();
            return true;
        }
        if (k == juce::KeyPress::spaceKey && ! search.hasKeyboardFocus(true))
        {
            toggleAudition();
            return true;
        }
        if (k == juce::KeyPress::returnKey && ! search.hasKeyboardFocus(true))
        {
            load();
            return true;
        }
        return false;
    }
}
