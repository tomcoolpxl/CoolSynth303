#include "ui/Panel.h"

#include <array>
#include <cmath>

#include "Editor.h"
#include "Processor.h"
#include "ui/Theme.h"

namespace cs303::ui
{
    using Knob_t = cs303::Knob; // the pattern's knob names, beside the panel's Knob widget

    namespace
    {
        constexpr int faceTop = 10, sectionHeight = 170;
    }

    // ---------------------------------------------------------------- the top bar
    TopBar::TopBar(Editor& e) : editor(e)
    {
        auto& p = editor.processor;
        prev = &add("<");
        prev->onClick = [this] { editor.choosePattern((editor.editPattern + 15) % 16, editor.editB); };
        next = &add(">");
        next->onClick = [this] { editor.choosePattern((editor.editPattern + 1) % 16, editor.editB); };
        add("New").onClick = [this, &p] {
            p.edit([](Project& pr) { pr = defaultProject(); }); // one undo step: a stray click loses nothing
            editor.pick = 0;
            editor.choosePattern(0, false);
        };
        add("Open").onClick = [this] { openProject(); };
        add("Save").onClick = [this] { saveProject(); };
        play = &add("Play");
        play->textOf = [&p] { return juce::String(p.isPlaying() ? "Stop" : "Play"); };
        play->isOn = [&p] { return p.isPlaying(); };
        play->onClick = [this] { editor.togglePlay(); };
        add("Tap").onClick = [this] { tap(); };
        rec = &add("Rec");
        rec->isOn = [&p] { return p.isRecording(); };
        rec->onClick = [&p] { p.setRecording(! p.isRecording()); };
        mods = &add("Mods");
        mods->isOn = [this] { return editor.modsOpen; };
        mods->onClick = [this] { editor.setModsOpen(! editor.modsOpen); };
    }

    Key& TopBar::add(const juce::String& text)
    {
        keys.push_back(std::make_unique<Key>(text, false));
        addAndMakeVisible(*keys.back());
        return *keys.back();
    }

    void TopBar::resized()
    {
        const int y = 8, h = 26;
        int x = 60;
        keys[0]->setBounds(x, y, 30, h);
        nameBox = { x + 36, y, 210, h };
        x = nameBox.getRight() + 6;
        keys[1]->setBounds(x, y, 30, h);
        x += 40;
        for (int i = 2; i <= 4; ++i)
        {
            keys[static_cast<size_t>(i)]->setBounds(x, y, 56, h);
            x += 62;
        }
        int r = getWidth() - 16;
        mods->setBounds(r - 60, y, 60, h);
        r -= 76;
        rec->setBounds(r - 50, y, 50, h);
        r -= 58;
        keys[6]->setBounds(r - 50, y, 50, h); // Tap
        r -= 58;
        tempoBox = { r - 104, y, 104, h };
        r -= 112;
        play->setBounds(r - 64, y, 64, h);
    }

    void TopBar::paint(juce::Graphics& g)
    {
        g.fillAll(colour::bar);
        g.setColour(juce::Colours::black);
        g.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(getWidth()));
        const auto& proj = editor.processor.project();
        const auto& pat = editor.edited();
        const juce::String name = juce::String(proj.name) + "  ·  " + juce::String(editor.editPattern + 1).paddedLeft('0', 2)
                                  + (editor.editB ? "B" : "A") + (pat.name.empty() ? juce::String() : "  " + juce::String(pat.name));
        g.setColour(colour::well);
        g.fillRoundedRectangle(nameBox.toFloat(), 3.0f);
        g.setColour(colour::wellLine);
        g.drawRoundedRectangle(nameBox.toFloat().reduced(0.5f), 3.0f, 1.0f);
        g.setColour(juce::Colour(0xffe6e6e6));
        g.setFont(bodyFont(12.5f));
        g.drawText(name, nameBox.reduced(8, 0), juce::Justification::centred, true);

        // following an incoming MIDI clock, its tempo, named so
        const bool byHost = editor.processor.isFollowingHost();
        const bool byClock = byHost || editor.processor.isFollowingClock();
        g.setColour(colour::orange);
        g.setFont(monoFont(17.0f));
        g.drawText(juce::String(byClock ? editor.processor.tempoNow() : proj.bpm, 1), tempoBox.withTrimmedRight(40), juce::Justification::centredRight, false);
        g.setColour(byClock ? colour::orange : colour::hint);
        g.setFont(labelFont(10.5f));
        g.drawText(byHost ? "HOST" : byClock ? "CLOCK" : "BPM", tempoBox.withTrimmedLeft(68), juce::Justification::centredLeft, false);

        // the audio thread's share of its time, small, at the far left of the transport
        const float load = editor.processor.load();
        g.setColour(load > 0.8f ? colour::lampOn : colour::dim);
        g.setFont(monoFont(9.5f));
        g.drawText("DSP " + juce::String(juce::roundToInt(load * 100.0f)) + "%", juce::Rectangle<int>(play->getX() - 74, 8, 64, 26),
                   juce::Justification::centredRight, false);
    }

    void TopBar::mouseDown(const juce::MouseEvent& e)
    {
        dragStartBpm = editor.processor.project().bpm;
        if (tempoBox.contains(e.getPosition()) && e.getNumberOfClicks() == 2)
            editor.processor.edit([](Project& p) { p.bpm = 128.0; }, true);
    }

    void TopBar::mouseDrag(const juce::MouseEvent& e)
    {
        if (! tempoBox.contains(e.getMouseDownPosition()))
            return;
        const double fine = e.mods.isShiftDown() ? 0.1 : 0.5;
        const double bpm = juce::jlimit(40.0, 300.0, std::round((dragStartBpm - e.getDistanceFromDragStartY() * fine) * 10.0) / 10.0);
        editor.processor.edit([bpm](Project& p) { p.bpm = bpm; }, false);
        repaint();
    }

    void TopBar::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
    {
        if (! tempoBox.contains(e.getPosition()))
            return;
        const double step = (w.deltaY > 0.0f ? 1.0 : -1.0) * (e.mods.isShiftDown() ? 0.1 : 1.0);
        editor.processor.edit([step](Project& p) { p.bpm = juce::jlimit(40.0, 300.0, p.bpm + step); }, false);
        repaint();
    }

    void TopBar::tap()
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (! taps.empty() && now - taps.back() > 2000.0)
            taps.clear();
        taps.push_back(now);
        if (taps.size() > 5)
            taps.erase(taps.begin());
        if (taps.size() >= 2)
        {
            const double gap = (taps.back() - taps.front()) / static_cast<double>(taps.size() - 1);
            const double bpm = juce::jlimit(40.0, 300.0, std::round(600000.0 / gap) / 10.0);
            editor.processor.edit([bpm](Project& p) { p.bpm = bpm; }, false);
        }
        repaint();
    }

    void TopBar::openProject()
    {
        chooser = std::make_unique<juce::FileChooser>("Open a CoolSynth 303 project", juce::File(), "*.cs303");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& c)
        {
            const auto file = c.getResult();
            if (! file.existsAsFile())
                return;
            auto p = projectFromJson(file.loadFileAsString());
            p.name = file.getFileNameWithoutExtension().toStdString();
            editor.processor.replaceProject(p);
            editor.choosePattern(0, false);
        });
    }

    void TopBar::saveProject()
    {
        chooser = std::make_unique<juce::FileChooser>("Save the project", juce::File(), "*.cs303");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this](const juce::FileChooser& c)
        {
            auto file = c.getResult();
            if (file == juce::File())
                return;
            file = file.withFileExtension("cs303");
            editor.processor.edit([&](Project& p) { p.name = file.getFileNameWithoutExtension().toStdString(); }, false);
            file.replaceWithText(toJson(editor.processor.project()));
            repaint();
        });
    }

    void TopBar::refresh()
    {
        repaint();
        for (auto& k : keys)
            k->repaint();
    }

    // ---------------------------------------------------------------- the Mods panel
    ModsPanel::ModsPanel(Editor& e) : editor(e)
    {
        auto& p = editor.processor;
        struct Trim
        {
            const char* name;
            float Panel::*field;
            float lo, hi, stock;
        };
        const Panel stock;
        const Trim trims[] = { { "TM3", &Panel::tm3, 0.0f, 1.0f, stock.tm3 },
                               { "TM4", &Panel::tm4, 0.0f, 1.0f, stock.tm4 },
                               { "TM5", &Panel::tm5, 0.0f, 1.0f, stock.tm5 },
                               { "C11", &Panel::c11, c11Lowest, c11Highest, stock.c11 } };
        int x = 14;
        for (const auto& t : trims)
        {
            auto k = std::make_unique<Knob>(t.name, 34.0f);
            const auto field = t.field;
            const float lo = t.lo, hi = t.hi;
            k->get = [&p, field, lo, hi] { return ((p.project().panel.*field) - lo) / (hi - lo); };
            k->set = [&p, field, lo, hi](float v) { p.edit([&](Project& pr) { pr.panel.*field = lo + v * (hi - lo); }); };
            k->defaultValue = (t.stock - lo) / (hi - lo);
            k->showRange = false;
            k->gestureStart = [&p] { p.beginGesture(); };
            k->gestureEnd = [&p] { p.endGesture(); };
            const auto b = k->preferredBounds(0, 0);
            k->setBounds(x, faceTop + 22, b.getWidth() + 14, b.getHeight());
            addAndMakeVisible(*k);
            parts.push_back(std::move(k));
            auto value = std::make_unique<Caption>();
            const bool isC11 = field == &Panel::c11;
            value->text = [&p, field, isC11] {
                const float v = p.project().panel.*field;
                return isC11 ? juce::String(v, 2) + " uF" : juce::String(v, 3);
            };
            value->font = monoFont(11.0f);
            value->ink = colour::orange;
            value->justification = juce::Justification::centred;
            value->setBounds(x, faceTop + 22 + b.getHeight() + 2, b.getWidth() + 14, 16);
            addAndMakeVisible(*value);
            parts.push_back(std::move(value));
            x += 76;
        }
        auto stockKey = std::make_unique<Key>("Stock", true);
        stockKey->onClick = [&p] {
            p.edit([](Project& pr) {
                const Panel s;
                pr.panel.tm3 = s.tm3;
                pr.panel.tm4 = s.tm4;
                pr.panel.tm5 = s.tm5;
                pr.panel.c11 = s.c11;
            });
        };
        stockKey->isOn = [&p] {
            const Panel s;
            const auto& pn = p.project().panel;
            return pn.tm3 == s.tm3 && pn.tm4 == s.tm4 && pn.tm5 == s.tm5 && pn.c11 == s.c11;
        };
        stockKey->setBounds(14, faceTop + 124, 70, 24 + Key::lampRoom);
        addAndMakeVisible(*stockKey);
        parts.push_back(std::move(stockKey));
        auto trimNote = std::make_unique<Caption>();
        trimNote->multiline = true;
        trimNote->font = bodyFont(11.0f);
        trimNote->ink = colour::hint;
        trimNote->justification = juce::Justification::topLeft;
        trimNote->text = [] {
            return juce::String("Inside the case: TM3 the cutoff's reach, TM4 the tune, TM5 an octave's width, C11 the "
                                "square's shaper.");
        };
        trimNote->setBounds(96, faceTop + 122, 214, 44);
        addAndMakeVisible(*trimNote);
        parts.push_back(std::move(trimNote));

        buildDevilFish();

        auto close = std::make_unique<Key>("Close", false);
        close->onClick = [this] { editor.setModsOpen(false); };
        close->setBounds(1112, faceTop + 132, 84, 24);
        addAndMakeVisible(*close);
        parts.push_back(std::move(close));
    }

    namespace
    {
        /** The Devil Fish's red Accent button: every note accented while it is held. */
        class AccentButton final : public juce::Component
        {
        public:
            explicit AccentButton(Processor& p) : processor(p) {}
            std::function<bool()> enabled;
            void paint(juce::Graphics& g) override
            {
                const bool on = enabled == nullptr || enabled();
                const auto r = juce::Rectangle<float>(0.0f, 0.0f, 30.0f, 30.0f).withCentre({ getWidth() * 0.5f, 17.0f });
                g.setColour(juce::Colours::black.withAlpha(0.5f));
                g.fillEllipse(r.translated(0.0f, 1.5f));
                const auto red = juce::Colour(0xffd8261c).withMultipliedBrightness(held ? 0.75f : 1.0f);
                g.setColour(on ? red : red.withSaturation(0.25f).withMultipliedBrightness(0.5f));
                g.fillEllipse(r);
                g.setColour(juce::Colours::white.withAlpha(0.18f));
                g.fillEllipse(r.reduced(6.0f).translated(-3.0f, -4.0f));
                g.setColour(on ? colour::label : colour::dim);
                g.setFont(labelFont(10.5f));
                g.drawText("ACCENT", getLocalBounds().removeFromBottom(14), juce::Justification::centred, false);
            }
            void mouseDown(const juce::MouseEvent&) override
            {
                if (enabled != nullptr && !enabled())
                    return;
                held = true;
                processor.holdAccent(true);
                repaint();
            }
            void mouseUp(const juce::MouseEvent&) override
            {
                held = false;
                processor.holdAccent(false);
                repaint();
            }

        private:
            Processor& processor;
            bool held = false;
        };
    }

    void ModsPanel::buildDevilFish()
    {
        auto& p = editor.processor;
        const auto fishOn = [&p] { return p.project().panel.devilFish; };

        auto onKey = std::make_unique<Key>("On", true);
        onKey->isOn = fishOn;
        onKey->onClick = [&p] { p.edit([](Project& pr) { pr.panel.devilFish = !pr.panel.devilFish; }); };
        onKey->setBounds(fishX + 14, faceTop + 22, 58, 24 + Key::lampRoom);
        addAndMakeVisible(*onKey);
        parts.push_back(std::move(onKey));

        auto accent = std::make_unique<AccentButton>(p);
        accent->enabled = fishOn;
        accent->setBounds(fishX + 10, faceTop + 76, 66, 50);
        addAndMakeVisible(*accent);
        parts.push_back(std::move(accent));

        // Each knob with what it sets, in the units Whittle gives (Tb303Voice::DevilFish for the scales).
        const auto ms = [](double s) {
            return s < 0.01 ? juce::String(s * 1000.0, 1) + " ms"
                            : s < 1.0 ? juce::String(juce::roundToInt(s * 1000.0)) + " ms" : juce::String(s, 2) + " s";
        };
        struct FishKnob
        {
            const char* name;
            float Panel::*field;
            std::function<juce::String(float)> value;
        };
        const FishKnob knobs[] = {
            { "Overdrive", &Panel::overdrive,
              [](float t) {
                  const double g = t < 0.2f ? t / 0.2 : std::pow(66.6, (t - 0.2) / 0.8);
                  return juce::String::charToString(0xd7) + juce::String(g, g < 10.0 ? 1 : 0);
              } },
            { "Slide Time", &Panel::slideTime, [ms](float t) { return ms(0.06 * (103.3 + 500.0 * t) / 103.3); } },
            { "Soft Attack", &Panel::softAttack, [ms](float t) { return ms(0.3e-3 * std::pow(100.0, t)); } },
            { "Norm Decay", &Panel::normalDecay, [ms](float t) { return ms(0.03 * std::pow(100.0, t)); } },
            { "Acc Decay", &Panel::accentDecay, [ms](float t) { return ms(0.03 * std::pow(100.0, t)); } },
            { "Tracking", &Panel::tracking, [](float t) { return juce::String(1.5 * t, 2) + " oct/V"; } },
            { "Filter FM", &Panel::filterFm, [](float t) { return juce::String(juce::roundToInt(t * 10.0f)) + " / 10"; } },
        };
        int x = fishX + 84;
        const Panel stock;
        for (const auto& k : knobs)
        {
            auto knob = std::make_unique<Knob>(k.name, 34.0f);
            const auto field = k.field;
            knob->get = [&p, field] { return p.project().panel.*field; };
            knob->set = [&p, field](float v) { p.edit([&](Project& pr) { pr.panel.*field = v; }); };
            knob->defaultValue = stock.*field;
            knob->showRange = false;
            knob->dimmed = [fishOn] { return !fishOn(); };
            knob->gestureStart = [&p] { p.beginGesture(); };
            knob->gestureEnd = [&p] { p.endGesture(); };
            const auto b = knob->preferredBounds(0, 0);
            knob->setBounds(x, faceTop + 22, b.getWidth() + 18, b.getHeight());
            addAndMakeVisible(*knob);
            parts.push_back(std::move(knob));
            auto value = std::make_unique<Caption>();
            const auto shown = k.value;
            value->text = [&p, field, shown] { return shown(p.project().panel.*field); };
            value->font = monoFont(11.0f);
            value->ink = colour::orange;
            value->justification = juce::Justification::centred;
            value->setBounds(x - 6, faceTop + 22 + b.getHeight() + 2, b.getWidth() + 30, 16);
            addAndMakeVisible(*value);
            parts.push_back(std::move(value));
            x += 82;
        }

        // The three switches: a key each, clicked through its positions (right-click back).
        struct FishSwitch
        {
            const char* name;
            int Panel::*field;
            std::array<const char*, 3> positions;
        };
        const FishSwitch switches[] = {
            { "Sweep Speed", &Panel::sweepSpeed, { "Fast", "Normal", "Slow" } },
            { "Acc Sweep", &Panel::accentSweep, { "Off, high res", "High res", "Normal" } },
            { "Muffler", &Panel::muffler, { "Off", "Soft", "Hard" } },
        };
        int y = faceTop + 22;
        for (const auto& s : switches)
        {
            auto key = std::make_unique<Key>(s.name, false);
            const auto field = s.field;
            const juce::String name = s.name;
            const auto positions = s.positions;
            key->textOf = [&p, field, name, positions] {
                return name + ": " + positions[static_cast<size_t>(juce::jlimit(0, 2, p.project().panel.*field))];
            };
            key->onClick = [&p, field] { p.edit([&](Project& pr) { pr.panel.*field = (pr.panel.*field + 1) % 3; }); };
            key->onRightClick = [&p, field] { p.edit([&](Project& pr) { pr.panel.*field = (pr.panel.*field + 2) % 3; }); };
            key->enabled = fishOn;
            key->setBounds(fishRight - 196, y, 184, 26);
            addAndMakeVisible(*key);
            parts.push_back(std::move(key));
            y += 32;
        }

        auto fishStock = std::make_unique<Key>("Stock", true);
        fishStock->onClick = [&p] {
            p.edit([](Project& pr) {
                const Panel s;
                auto& pn = pr.panel;
                pn.overdrive = s.overdrive;
                pn.slideTime = s.slideTime;
                pn.softAttack = s.softAttack;
                pn.normalDecay = s.normalDecay;
                pn.accentDecay = s.accentDecay;
                pn.tracking = s.tracking;
                pn.filterFm = s.filterFm;
                pn.sweepSpeed = s.sweepSpeed;
                pn.accentSweep = s.accentSweep;
                pn.muffler = s.muffler;
                pn.decay = 0.5f; // a Devil Fish's DECAY is the volume envelope's: the TB-303's at half
            });
        };
        fishStock->isOn = [&p] {
            const Panel s;
            const auto& pn = p.project().panel;
            return pn.overdrive == s.overdrive && pn.slideTime == s.slideTime && pn.softAttack == s.softAttack
                   && pn.normalDecay == s.normalDecay && pn.accentDecay == s.accentDecay && pn.tracking == s.tracking
                   && pn.filterFm == s.filterFm && pn.sweepSpeed == s.sweepSpeed && pn.accentSweep == s.accentSweep
                   && pn.muffler == s.muffler && pn.decay == 0.5f;
        };
        fishStock->enabled = fishOn;
        fishStock->setBounds(fishRight - 196, faceTop + 124, 76, 24 + Key::lampRoom);
        addAndMakeVisible(*fishStock);
        parts.push_back(std::move(fishStock));

        auto note = std::make_unique<Caption>();
        note->multiline = true;
        note->font = bodyFont(11.0f);
        note->ink = colour::hint;
        note->justification = juce::Justification::topLeft;
        note->text = [fishOn] {
            return fishOn() ? juce::String("Robin Whittle's mod: the panel's Decay now sets the volume's decay (held notes past "
                                           "half), Env Mod and Cut Off reach further, the bass is fuller. Stock sets the "
                                           "knobs where it sounds most like a TB-303.")
                            : juce::String("Off: the stock TB-303, untouched. On: Robin Whittle's Devil Fish mod, with these "
                                           "controls and wider Cut Off and Env Mod ranges.");
        };
        note->setBounds(fishX + 92, faceTop + 118, fishRight - 196 - fishX - 104, 48);
        addAndMakeVisible(*note);
        parts.push_back(std::move(note));
    }

    void ModsPanel::paint(juce::Graphics& g)
    {
        g.fillAll(colour::face);
        drawSection(g, juce::Rectangle<int>(6, faceTop, fishX - 14, sectionHeight).toFloat(), "Trimmers", colour::face);
        const auto fish = juce::Rectangle<int>(fishX, faceTop, fishRight - fishX, sectionHeight).toFloat();
        drawSection(g, fish, {}, colour::face);
        // The name in red, as the Devil Fish's own panel prints it.
        const auto font = juce::Font(juce::FontOptions(juce::Font::getDefaultSerifFontName(), 17.0f,
                                                       juce::Font::bold | juce::Font::italic));
        const juce::String name = "Devil Fish";
        const float w = juce::GlyphArrangement::getStringWidth(font, name) + 14.0f;
        const juce::Rectangle<float> gap(fish.getX() + 10.0f, fish.getY() - 10.0f, w, 20.0f);
        g.setColour(colour::face);
        g.fillRect(gap);
        g.setColour(juce::Colour(0xffe0362a));
        g.setFont(font);
        g.drawText(name, gap, juce::Justification::centred, false);
    }

    void ModsPanel::refresh()
    {
        for (auto& c : parts)
            c->repaint();
    }

    // ---------------------------------------------------------------- the synth's sections
    SynthPanel::SynthPanel(Editor& e) : editor(e)
    {
        auto& p = editor.processor;
        sections = { { "Oscillator", 6, 150 }, { "Filter", 178, 280 }, { "Envelope", 480, 176 }, { "Amplifier", 678, 104 },
                     { "Drive", 804, 170 }, { "Delay", 996, 214 } };

        const auto panelKnob = [this, &p](Knob& k, Knob_t which) {
            const auto i = static_cast<int>(which);
            k.get = [&p, i] {
                const auto& pn = p.project().panel;
                const float v[] = { pn.cutoff, pn.resonance, pn.envMod, pn.decay, pn.accent };
                return v[i];
            };
            k.set = [&p, which](float v) { p.knobMoved(which, v); };
            k.follow = [&p, which] { return p.isPlaying() ? p.liveKnob(which) : -1.0f; };
            k.gestureStart = [&p] { p.beginGesture(); };
            k.gestureEnd = [&p] { p.endGesture(); };
        };
        const auto plainKnob = [&p](Knob& k, float Panel::*field, bool undoable = true) {
            k.get = [&p, field] { return p.project().panel.*field; };
            k.set = [&p, field, undoable](float v) { p.edit([&](Project& pr) { pr.panel.*field = v; }, undoable); };
            k.gestureStart = [&p] { p.beginGesture(); };
            k.gestureEnd = [&p] { p.endGesture(); };
        };

        // the oscillator
        plainKnob(knob("Tuning", 44.0f, 6, 50, 28), &Panel::tuning);
        {
            auto& saw = key("Saw", 6, 16, 120, 0);
            saw.isOn = [&p] { return ! p.project().panel.square; };
            saw.onClick = [&p] { p.edit([](Project& pr) { pr.panel.square = false; }); };
            auto& sq = key("Square", 6, 16 + saw.getWidth() + 6, 120, 0);
            sq.isOn = [&p] { return p.project().panel.square; };
            sq.onClick = [&p] { p.edit([](Project& pr) { pr.panel.square = true; }); };
        }
        // the filter
        {
            auto& cutoff = knob("Cutoff", 60.0f, 178, 22, 34);
            panelKnob(cutoff, Knob_t::cutoff);
            panelKnob(knob("Resonance", 44.0f, 178, 120, 40), Knob_t::resonance);
            panelKnob(knob("Env mod", 44.0f, 178, 204, 40), Knob_t::envMod);
        }
        // the envelope
        panelKnob(knob("Decay", 44.0f, 480, 20, 40), Knob_t::decay);
        panelKnob(knob("Accent", 44.0f, 480, 100, 40), Knob_t::accent);
        // the amplifier
        plainKnob(knob("Volume", 44.0f, 678, 26, 40), &Panel::volume);
        // the drive
        {
            auto& on = key("On", 804, 18, 120, 0);
            on.isOn = [&p] { return p.project().panel.driveOn; };
            on.onClick = [&p] { p.edit([](Project& pr) { pr.panel.driveOn = ! pr.panel.driveOn; }); };
            plainKnob(knob("Amount", 40.0f, 804, 16, 36), &Panel::drive);
            plainKnob(knob("Tone", 40.0f, 804, 94, 36), &Panel::tone);
        }
        // the delay
        {
            auto& on = key("On", 996, 14, 120, 0);
            on.isOn = [&p] { return p.project().panel.delayOn; };
            on.onClick = [&p] { p.edit([](Project& pr) { pr.panel.delayOn = ! pr.panel.delayOn; }); };
            auto& time = knob("Time", 40.0f, 996, 10, 36);
            time.get = [&p] { return static_cast<float>(p.project().panel.delayDivision) / 3.0f; };
            time.set = [&p](float v) {
                const int d = juce::roundToInt(v * 3.0f);
                if (d != p.project().panel.delayDivision)
                    p.edit([d](Project& pr) { pr.panel.delayDivision = d; });
            };
            time.defaultValue = 1.0f / 3.0f;
            auto value = std::make_unique<Caption>();
            value->text = [&p] { return juce::String(delayDivisionName(p.project().panel.delayDivision)); };
            value->font = monoFont(12.0f);
            value->ink = colour::orange;
            value->setBounds(996 + 62, faceTop + 128, 40, 24);
            addAndMakeVisible(*value);
            parts.push_back(std::move(value));
            for (auto [name, field, x] : { std::tuple { "Feedback", &Panel::feedback, 96 }, std::tuple { "Mix", &Panel::mix, 150 } })
            {
                auto f = std::make_unique<Fader>(name);
                f->get = [&p, field = field] { return p.project().panel.*field; };
                f->set = [&p, field = field](float v) { p.edit([&](Project& pr) { pr.panel.*field = v; }); };
                f->gestureStart = [&p] { p.beginGesture(); };
                f->gestureEnd = [&p] { p.endGesture(); };
                f->setBounds(996 + x - 15, faceTop + 18, 60, 140); // wide enough for its name under it
                addAndMakeVisible(*f);
                parts.push_back(std::move(f));
            }
        }
    }

    Knob& SynthPanel::knob(const juce::String& name, float size, int sectionX, int x, int y)
    {
        auto k = std::make_unique<Knob>(name, size);
        // the mockup places a knob by its corner; the dial's centre is what has to match
        const auto b = k->preferredBounds(0, 0);
        const int cx = sectionX + x + juce::roundToInt(size * 0.5f) + 8, cy = faceTop + y + juce::roundToInt(size * 0.5f) + 8;
        k->setBounds(cx - b.getWidth() / 2, cy - 12 - juce::roundToInt(size * 0.5f), b.getWidth(), b.getHeight());
        addAndMakeVisible(*k);
        auto& ref = *k;
        parts.push_back(std::move(k));
        return ref;
    }

    Key& SynthPanel::key(const juce::String& text, int sectionX, int x, int y, int w)
    {
        auto k = std::make_unique<Key>(text, true);
        const int width = w > 0 ? w : juce::roundToInt(juce::GlyphArrangement::getStringWidth(labelFont(10.5f), text.toUpperCase())) + 22;
        k->setBounds(sectionX + x, faceTop + y, std::max(40, width), 24 + Key::lampRoom);
        addAndMakeVisible(*k);
        auto& ref = *k;
        parts.push_back(std::move(k));
        return ref;
    }

    void SynthPanel::paint(juce::Graphics& g)
    {
        g.fillAll(colour::face);
        for (const auto& s : sections)
            drawSection(g, juce::Rectangle<int>(s.x, faceTop, s.w, sectionHeight).toFloat(), s.name, colour::face);
        g.setColour(colour::dim);
        g.setFont(bodyFont(13.0f));
        for (int x : { 160, 462, 660, 786, 978 })
            g.drawText(juce::String::charToString(0x25b8), juce::Rectangle<int>(x, 88, 14, 14), juce::Justification::centred, false);
    }

    void SynthPanel::refresh()
    {
        for (auto& c : parts)
            c->repaint();
    }
}
