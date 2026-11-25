#include "Editor.h"

#include "Library.h"
#include "Processor.h"
#include "StandaloneDevices.h"
#include "ui/Keyboard.h"
#include "ui/LibraryShelf.h"
#include "ui/LidView.h"
#include "ui/OptionsWindow.h"
#include "ui/Panel.h"
#include "ui/SequencerView.h"
#include "ui/Theme.h"
#include "ui/WindowLook.h"
#include "ui/Hints.h"

namespace cs303
{
    namespace
    {
        /** The board: the whole panel at its base size, scaled as one by the editor. */
        class Board final : public juce::Component
        {
        public:
            void paint(juce::Graphics& g) override
            {
                ui::drawGround(g, getLocalBounds().toFloat());
                // the name up the left edge, in tall capitals, engraved in a darker strip
                const juce::Rectangle<int> strip(0, 42, 52, Editor::baseHeight - 42);
                g.setGradientFill(juce::ColourGradient(ui::colour::stencil, 0.0f, 0.0f, ui::palette().stencilRight, static_cast<float>(strip.getRight()), 0.0f, false));
                g.fillRect(strip);
                g.setColour(juce::Colours::black);
                g.drawVerticalLine(strip.getRight(), static_cast<float>(strip.getY()), static_cast<float>(strip.getBottom()));
                g.setColour(juce::Colours::white.withAlpha(0.04f));
                g.drawVerticalLine(strip.getRight() + 1, static_cast<float>(strip.getY()), static_cast<float>(strip.getBottom()));
                {
                    juce::Graphics::ScopedSaveState state(g);
                    g.addTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi, static_cast<float>(strip.getCentreX()),
                                                                   static_cast<float>(strip.getCentreY())));
                    const auto font = ui::fontOf(ui::Face::head, 40.0f, 0.08f);
                    const auto c = strip.getCentre().toFloat();
                    const juce::Rectangle<float> place(c.x - 300.0f, c.y - 26.0f, 600.0f, 52.0f);
                    const auto title = [&](juce::Colour a, juce::Colour b, juce::Point<float> by) {
                        juce::AttributedString s;
                        s.append("COOLSYNTH ", font, a);
                        s.append("303", font, b);
                        s.setJustification(juce::Justification::centred);
                        s.draw(g, place + by);
                    };
                    title(juce::Colours::black, juce::Colours::black, { 0.0f, -1.0f });                                   // the engraving's dark edge
                    title(juce::Colours::white.withAlpha(0.08f), juce::Colours::white.withAlpha(0.08f), { 0.0f, 1.0f });  // and its lit one
                    title(ui::palette().titleInk, ui::colour::orange, {});
                }
            }
        };

        // the computer's keys: Z to comma, one octave from the middle C (the piano's key 12)
        int keyForCharacter(int c)
        {
            const juce::String row = "ZSXDCVGBHNJM,";
            const int i = row.indexOfChar(static_cast<juce::juce_wchar>(juce::CharacterFunctions::toUpperCase(static_cast<juce::juce_wchar>(c))));
            return i < 0 ? -1 : 12 + i;
        }
    }

    Editor::Editor(Processor& p) : juce::AudioProcessorEditor(p), processor(p)
    {
        {
            // the theme kept from last time (D352), before anything is drawn
            const juce::PropertiesFile settings(Processor::settingsOptions());
            ui::applyTheme(settings.getIntValue("theme", 0));
            ui::WindowLook::shared().recolour();
        }
        setLookAndFeel(&ui::WindowLook::shared()); // pop-up menus, scroll bars and text fields in the panel's look
        boardHolder = std::make_unique<Board>();
        addAndMakeVisible(*boardHolder);
        top = std::make_unique<ui::TopBar>(*this);
        synth = std::make_unique<ui::SynthPanel>(*this);
        keys = std::make_unique<ui::KeyboardSection>(*this);
        sequencer = std::make_unique<ui::SequencerSection>(*this);
        for (juce::Component* c : { static_cast<juce::Component*>(top.get()), static_cast<juce::Component*>(synth.get()),
                                    static_cast<juce::Component*>(keys.get()), static_cast<juce::Component*>(sequencer.get()) })
            boardHolder->addAndMakeVisible(*c);
        mods = std::make_unique<ui::ModsPanel>(*this);
        boardHolder->addChildComponent(*mods);
        lid = std::make_unique<ui::LidView>(p);
        lid->onClose = [this] { setLidOpen(false); };
        addChildComponent(*lid);
        layOut();
        setWantsKeyboardFocus(true);
        setResizable(true, true);
        setResizeLimits(640, 400, 3840, 2400);
        // the base size, or smaller to fit the screen (a laptop at 150 % has 1280 by 720 to give), its shape kept
        float fit = 1.0f;
        if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        {
            const auto area = display->userBounds.reduced(24.0f, 48.0f);
            fit = std::min({ 1.0f, area.getWidth() / static_cast<float>(baseWidth), area.getHeight() / static_cast<float>(baseHeight) });
        }
        // a size chosen in the options (D349) instead, in the app and a plugin (the tests keep the fit)
        if (processor.wrapperType != juce::AudioProcessor::wrapperType_Undefined)
        {
            const juce::PropertiesFile settings(Processor::settingsOptions());
            if (const int kept = settings.getIntValue("windowSize", 0); kept > 0)
                fit = juce::jlimit(50, 300, kept) / 100.0f;
        }
        setSize(std::max(640, juce::roundToInt(baseWidth * fit)), std::max(400, juce::roundToInt(baseHeight * fit)));
        auto line = std::make_unique<ui::HintLine>(false);
        line->setBounds(64, 778, 1200, 20);
        boardHolder->addAndMakeVisible(*line);
        hintWatcher = std::make_unique<ui::HintWatcher>(*boardHolder, *line);
        boardHolder->addMouseListener(hintWatcher.get(), true);
        hintLine = std::move(line);
        startTimerHz(30);
    }

    juce::String Editor::hintAt(juce::Point<int> onBoard) { return ui::hintOnBoard(*boardHolder, onBoard); }

    Editor::~Editor()
    {
        boardHolder->removeMouseListener(hintWatcher.get());
        setLookAndFeel(nullptr);
        stopTimer();
        optionsWindow.reset(); // its hooks point here
        if (watchedWindow != nullptr)
            watchedWindow->removeComponentListener(this);
        stopAudition(); // the project as it was, before the window goes
        library = nullptr;
        lid->setOpen(false);
        processor.audition(-1);
    }

    void Editor::layOut()
    {
        boardHolder->setBounds(0, 0, baseWidth, baseHeight);
        top->setBounds(0, 0, baseWidth, 42);
        synth->setBounds(60, 52, 1210, 186);
        mods->setBounds(60, 52, 1210, 186);
        keys->setBounds(60, 246, 1210, 130);
        sequencer->setBounds(60, 384, 1210, 392);
    }

    void Editor::paint(juce::Graphics& g)
    {
        g.fillAll(ui::colour::desk);
    }

    void Editor::resized()
    {
        const float scale = std::min(getWidth() / static_cast<float>(baseWidth), getHeight() / static_cast<float>(baseHeight));
        const float x = (getWidth() - baseWidth * scale) * 0.5f, y = (getHeight() - baseHeight * scale) * 0.5f;
        boardHolder->setTransform(juce::AffineTransform::scale(scale).translated(x, y));
        lid->setBounds(getLocalBounds());
    }

    // ---------------------------------------------------------------- the options (D349)
    bool Editor::optionsOpen() const noexcept { return optionsWindow != nullptr && optionsWindow->isVisible(); }

    ui::OptionsHooks Editor::optionsHooks()
    {
        ui::OptionsHooks hooks;
        hooks.accuracy = [this] { return processor.accuracy(); };
        hooks.chooseAccuracy = [this](int level) { processor.setAccuracy(level); }; // kept by the processor in its settings
        hooks.windowSize = [this] { return windowSizePercent(); };
        hooks.chooseWindowSize = [this](int percent) { chooseWindowSize(percent); };
        hooks.theme = [] { return ui::currentTheme(); };
        hooks.chooseTheme = [this](int index) { chooseTheme(index); };
        return hooks;
    }

    void Editor::toggleOptions()
    {
        if (optionsOpen())
        {
            optionsWindow.reset();
            top->refresh();
            return;
        }
        optionsWindow = std::make_unique<ui::OptionsWindow>(optionsHooks(), standaloneDeviceManager(), Processor::settingsOptions());
        optionsWindow->onClose = [this]
        {
            // after the click that asked has returned
            juce::MessageManager::callAsync([safe = juce::Component::SafePointer<Editor>(this)]
            {
                if (safe != nullptr && safe->optionsOpen())
                    safe->toggleOptions();
            });
        };
        optionsWindow->keepAbove(*this);
        optionsWindow->setVisible(true);
        optionsWindow->toFront(true);
        // It stays above the 303's window: brought to the front whenever that window is (not above other apps).
        if (auto* window = getTopLevelComponent(); window != nullptr && window != this && window != watchedWindow)
        {
            if (watchedWindow != nullptr)
                watchedWindow->removeComponentListener(this);
            watchedWindow = window;
            watchedWindow->addComponentListener(this);
        }
        top->refresh();
    }

    void Editor::componentBroughtToFront(juce::Component&)
    {
        if (optionsOpen())
            optionsWindow->toFront(false);
    }

    void Editor::chooseTheme(int index)
    {
        applyChosenTheme(index);
        juce::PropertiesFile settings(Processor::settingsOptions());
        settings.setValue("theme", ui::currentTheme());
        settings.saveIfNeeded();
    }

    void Editor::applyChosenTheme(int index)
    {
        ui::applyTheme(index);
        ui::WindowLook::shared().recolour();
        refreshAll(true);
        repaint(); // every part of the panel, the library and the lid with it
        if (optionsWindow != nullptr)
            optionsWindow->repaint();
    }

    int Editor::windowSizePercent() const
    {
        const float scale = std::min(getWidth() / static_cast<float>(baseWidth), getHeight() / static_cast<float>(baseHeight));
        return juce::roundToInt(scale * 100.0f);
    }

    void Editor::chooseWindowSize(int percent)
    {
        percent = juce::jlimit(50, 300, percent);
        if (processor.wrapperType != juce::AudioProcessor::wrapperType_Undefined)
        {
            juce::PropertiesFile settings(Processor::settingsOptions());
            settings.setValue("windowSize", percent);
            settings.saveIfNeeded();
        }
        setSize(baseWidth * percent / 100, baseHeight * percent / 100);
    }

    void Editor::setLidOpen(bool open)
    {
        lid->setBounds(getLocalBounds());
        lid->setVisible(open);
        lid->setOpen(open);
        if (open)
            lid->toFront(true);
        else
            grabKeyboardFocus();
    }

    void Editor::openLibrary()
    {
        if (library != nullptr)
            return;
        library = std::make_unique<ui::LibraryShelf>();
        const auto two = juce::String(editPattern + 1).paddedLeft('0', 2) + (editB ? "B" : "A");
        library->loadTarget = two;
        library->onClose = [this]
        {
            // Let go after the shelf's own callback has returned, never inside it.
            juce::MessageManager::callAsync([safe = juce::Component::SafePointer<Editor>(this)] { if (safe != nullptr) safe->closeLibrary(); });
        };
        library->onLoadLine = [this](const LibraryLine& l) { loadLine(l); };
        library->onLoadSound = [this](const LibrarySound& s) { loadSound(s); };
        library->onAuditionLine = [this](const LibraryLine* l) { auditionLine(l); };
        library->onAuditionSound = [this](const LibrarySound* s) { auditionSound(s); };
        library->auditionActive = [this] { return auditionSaved != nullptr; };
        library->currentLine = [this]
        {
            LibraryLine l;
            l.name = "Pattern " + juce::String(editPattern + 1).paddedLeft('0', 2) + (editB ? "B" : "A");
            l.style = "My lines";
            l.bpm = processor.project().bpm;
            l.pattern = edited();
            return l;
        };
        library->currentSound = [this]
        {
            LibrarySound s;
            s.name = "My sound";
            s.family = "My sounds";
            s.values = soundValuesOf(processor.project().panel);
            return s;
        };
        boardHolder->addAndMakeVisible(*library);
        library->setBounds(0, 0, baseWidth, baseHeight);
        library->toFront(true);
        library->grabKeyboardFocus();
    }

    void Editor::closeLibrary()
    {
        stopAudition();
        library = nullptr;
        grabKeyboardFocus();
        refreshAll();
    }

    void Editor::beginAudition()
    {
        // The first entry heard keeps the project as it was; another after it starts again from that.
        if (auditionSaved == nullptr)
        {
            auditionSaved = std::make_unique<Project>(processor.project());
            auditionStartedPlaying = ! processor.willPlay();
        }
        else
        {
            const Project back = *auditionSaved;
            processor.edit([&back](Project& p) { p = back; }, false);
        }
    }

    void Editor::stopAudition()
    {
        if (auditionSaved == nullptr)
            return;
        const Project back = *auditionSaved;
        processor.edit([&back](Project& p) { p = back; }, false);
        if (auditionStartedPlaying)
            processor.setPlaying(false);
        auditionSaved.reset();
        auditionStartedPlaying = false;
        if (library != nullptr)
            library->auditionEnded();
        refreshAll();
    }

    void Editor::auditionLine(const LibraryLine* line)
    {
        if (line == nullptr)
        {
            stopAudition();
            return;
        }
        beginAudition();
        const int index = editPattern;
        const bool b = editB;
        const Pattern pattern = line->pattern;
        const double bpm = line->bpm;
        processor.edit([&](Project& p) { p.pattern(index, b) = pattern; p.bpm = bpm; }, false);
        processor.hearPattern(index, b);
        if (! processor.willPlay())
            processor.setPlaying(true);
        refreshAll();
    }

    void Editor::auditionSound(const LibrarySound* sound)
    {
        if (sound == nullptr)
        {
            stopAudition();
            return;
        }
        beginAudition();
        const auto values = sound->values;
        processor.edit([&values](Project& p) { p.panel = applySound(p.panel, values); }, false);
        if (! processor.willPlay())
            processor.setPlaying(true);
        refreshAll();
    }

    void Editor::loadLine(const LibraryLine& line)
    {
        // An audition gives yours back first, so the load is the one undo step from what you had; a transport the
        // audition started stops, as the 808's does.
        if (auditionSaved != nullptr)
        {
            const Project back = *auditionSaved;
            processor.edit([&back](Project& p) { p = back; }, false);
            auditionSaved.reset();
            if (auditionStartedPlaying)
                processor.setPlaying(false);
            auditionStartedPlaying = false;
        }
        const int index = editPattern;
        const bool b = editB;
        const Pattern pattern = line.pattern;
        const double bpm = line.bpm;
        processor.edit([&](Project& p) { p.pattern(index, b) = pattern; p.bpm = bpm; });
        pick = std::min(pick, pattern.length - 1);
        if (library != nullptr)
            library->auditionEnded();
        refreshAll();
    }

    void Editor::loadSound(const LibrarySound& sound)
    {
        if (auditionSaved != nullptr)
        {
            const Project back = *auditionSaved;
            processor.edit([&back](Project& p) { p = back; }, false);
            auditionSaved.reset();
            if (auditionStartedPlaying)
                processor.setPlaying(false);
            auditionStartedPlaying = false;
        }
        const auto values = sound.values;
        processor.edit([&values](Project& p) { p.panel = applySound(p.panel, values); });
        if (library != nullptr)
            library->auditionEnded();
        refreshAll();
    }

    const Pattern& Editor::edited() const { return processor.project().pattern(editPattern, editB); }

    void Editor::editPatternWith(const std::function<void(Pattern&)>& change, bool undoable)
    {
        const int index = editPattern;
        const bool b = editB;
        processor.edit([&](Project& p) { change(p.pattern(index, b)); }, undoable);
        refreshAll();
    }

    void Editor::choosePattern(int index, bool variationB)
    {
        editPattern = juce::jlimit(0, Project::patterns - 1, index);
        editB = variationB;
        pick = std::min(pick, edited().length - 1);
        processor.hearPattern(editPattern, editB);
        refreshAll();
    }

    void Editor::playKey(int key)
    {
        if (transposeMode)
        {
            processor.setTranspose(key - 12);
            refreshAll();
            return;
        }
        const int at = pick;
        editPatternWith([at, key](Pattern& pat) {
            auto& s = pat.step[static_cast<size_t>(at)];
            s.setKey(key);
            s.gate = Gate::note;
        });
        movePick(1);
    }

    void Editor::setGateAtPick(Gate gate)
    {
        const int at = pick;
        editPatternWith([at, gate](Pattern& pat) { pat.step[static_cast<size_t>(at)].gate = gate; });
        movePick(1);
    }

    void Editor::togglePick(bool accent)
    {
        const int at = pick;
        editPatternWith([at, accent](Pattern& pat) {
            auto& s = pat.step[static_cast<size_t>(at)];
            (accent ? s.accent : s.slide) = ! (accent ? s.accent : s.slide);
        });
    }

    void Editor::movePick(int by)
    {
        const int n = std::max(1, edited().length);
        pick = ((pick + by) % n + n) % n;
        refreshAll();
    }

    void Editor::togglePlay()
    {
        processor.setPlaying(! processor.isPlaying());
    }

    void Editor::setModsOpen(bool open)
    {
        modsOpen = open;
        mods->setVisible(open);
        if (open)
            mods->toFront(false);
        refreshAll();
    }

    void Editor::refreshAll(bool everything)
    {
        lastEdits = processor.editCount(); // the timer need not draw this edit again
        top->refresh();
        keys->refresh();
        sequencer->refresh();
        const auto& pn = processor.project().panel;
        if (everything || ! panelShown || ! (pn == shownPanel))
        {
            shownPanel = pn;
            panelShown = true;
            synth->refresh();
            mods->refresh();
        }
    }

    bool Editor::keyPressed(const juce::KeyPress& key)
    {
        if (key == juce::KeyPress::spaceKey)
        {
            togglePlay();
            return true;
        }
        const auto held = key.getModifiers();
        if (held.isCommandDown() && (key.getKeyCode() == 'Z' || key.getKeyCode() == 'z'))
        {
            held.isShiftDown() ? processor.redo() : processor.undo();
            refreshAll();
            return true;
        }
        if (held.isCommandDown() && (key.getKeyCode() == 'Y' || key.getKeyCode() == 'y'))
        {
            processor.redo();
            refreshAll();
            return true;
        }
        if (held.isAnyModifierKeyDown())
            return false;
        const int k = keyForCharacter(key.getKeyCode());
        if (k >= 0 && k != heldComputerKey)
        {
            heldComputerKey = k;
            if (! processor.isPlaying())
                processor.audition(k);
            playKey(k);
            return true;
        }
        if (key == juce::KeyPress::leftKey)
        {
            movePick(-1);
            return true;
        }
        if (key == juce::KeyPress::rightKey)
        {
            movePick(1);
            return true;
        }
        return k >= 0;
    }

    bool Editor::keyStateChanged(bool isKeyDown)
    {
        if (! isKeyDown && heldComputerKey >= 0)
        {
            const juce::String row = "ZSXDCVGBHNJM,";
            const auto ch = row[heldComputerKey - 12];
            if (! juce::KeyPress::isKeyCurrentlyDown(static_cast<int>(ch)) && ! juce::KeyPress::isKeyCurrentlyDown(static_cast<int>(juce::CharacterFunctions::toLowerCase(ch))))
            {
                heldComputerKey = -1;
                processor.audition(-1);
                keys->refresh();
            }
        }
        return false;
    }

    void Editor::timerCallback()
    {
        const auto edits = processor.editCount();
        const bool playing = processor.isPlaying();
        const int step = processor.playingStep(), pattern = processor.playingPattern(), queued = processor.queuedPattern();
        const int sounding = processor.soundingKey();
        if (edits != lastEdits || playing != lastPlaying)
        {
            const bool started = playing != lastPlaying; // the knobs go back to their own values, or start to follow
            lastPlaying = playing;
            refreshAll(started);
        }
        else if (step != lastStep || pattern != lastPattern || queued != lastQueued || sounding != lastSounding)
        {
            // only what the beat moved: the two steps' columns, the pattern row when the pattern changes, the piano's
            // sounding key, the knobs the pattern turns
            sequencer->repaintPlaying(lastStep, step, pattern != lastPattern || queued != lastQueued);
            if (sounding != lastSounding)
                keys->repaintPiano();
            if (playing)
                synth->refreshMoving();
        }
        lastStep = step;
        lastPattern = pattern;
        lastQueued = queued;
        lastSounding = sounding;
        static int frame = 0;
        if (++frame % 15 == 0)
            top->repaint();
    }
}
