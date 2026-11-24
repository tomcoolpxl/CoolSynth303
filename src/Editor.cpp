#include "Editor.h"

#include "Library.h"
#include "Processor.h"
#include "ui/Keyboard.h"
#include "ui/LibraryShelf.h"
#include "ui/LidView.h"
#include "ui/Panel.h"
#include "ui/SequencerView.h"
#include "ui/Theme.h"

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
                g.fillAll(ui::colour::face);
                // the name up the left edge, in tall capitals
                const juce::Rectangle<int> strip(0, 42, 52, Editor::baseHeight - 42);
                g.setColour(ui::colour::stencil);
                g.fillRect(strip);
                g.setColour(juce::Colour(0xff111111));
                g.drawVerticalLine(strip.getRight(), static_cast<float>(strip.getY()), static_cast<float>(strip.getBottom()));
                {
                    juce::Graphics::ScopedSaveState state(g);
                    g.addTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi, static_cast<float>(strip.getCentreX()),
                                                                   static_cast<float>(strip.getCentreY())));
                    const auto font = ui::fontOf(ui::Face::head, 40.0f, 0.08f);
                    juce::AttributedString s;
                    s.append("COOLSYNTH ", font, juce::Colour(0xffd9d9d6));
                    s.append("303", font, ui::colour::orange);
                    s.setJustification(juce::Justification::centred);
                    const auto c = strip.getCentre().toFloat();
                    s.draw(g, juce::Rectangle<float>(c.x - 300.0f, c.y - 26.0f, 600.0f, 52.0f));
                }
                g.setColour(juce::Colour(0xff75777c));
                g.setFont(ui::bodyFont(10.5f));
                g.drawText("CoolSynth 303 " + juce::String::charToString(0xb7) + " a bass line from its circuit", juce::Rectangle<int>(Editor::baseWidth - 420, 780, 404, 16),
                           juce::Justification::centredRight, false);
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
        setSize(std::max(640, juce::roundToInt(baseWidth * fit)), std::max(400, juce::roundToInt(baseHeight * fit)));
        startTimerHz(30);
    }

    Editor::~Editor()
    {
        stopTimer();
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
            auditionStartedPlaying = ! processor.isPlaying();
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
        if (! processor.isPlaying())
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
        if (! processor.isPlaying())
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

    void Editor::refreshAll()
    {
        top->refresh();
        synth->refresh();
        mods->refresh();
        keys->refresh();
        sequencer->refresh();
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
            lastEdits = edits;
            lastPlaying = playing;
            refreshAll();
        }
        else if (step != lastStep || pattern != lastPattern || queued != lastQueued || sounding != lastSounding)
        {
            sequencer->repaintPlaying();
            keys->refresh();
            if (playing)
                synth->refresh(); // the knobs follow the pattern's locks and moves
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
