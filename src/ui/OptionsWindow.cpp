#include "ui/OptionsWindow.h"

#include <array>
#include <vector>

#include "StandaloneDevices.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "ui/WindowLook.h"

#if JUCE_WINDOWS
// The two calls an owned window needs, declared here rather than pulling all of windows.h in beside JUCE (as the 808's).
extern "C" __declspec(dllimport) std::intptr_t __stdcall SetWindowLongPtrW(void* window, int index, std::intptr_t value);
extern "C" __declspec(dllimport) int __stdcall SetWindowPos(void* window, void* after, int x, int y, int w, int h, unsigned int flags);
#endif

namespace cs303::ui
{
    namespace
    {
        // In pages, as the 808's (D335): the pages' list down the left, a page beside it; every page fits the one size.
        constexpr int panelWidth = 600, panelHeight = 372, sideWidth = 148, pad = 22, labelWidth = 104, rowHeight = 28, rowGap = 8,
                      headHeight = 22, topPad = 18, sidePad = 8, pagePitch = 33, pageHeight = 30;
        constexpr std::array<int, 6> windowSizes { 75, 100, 125, 150, 175, 200 };

        /** A value to pick from a list: the value in a dark box, a small arrow at its right; a click opens the list. */
        class Picker final : public juce::Component
        {
        public:
            std::function<juce::String()> value;
            std::function<juce::StringArray()> items;
            std::function<void(int)> choose;

            Picker() { setRepaintsOnMouseActivity(true); }

            void paint(juce::Graphics& g) override
            {
                const auto area = getLocalBounds().toFloat();
                g.setColour(colour::well);
                g.fillRoundedRectangle(area, 3.0f);
                g.setColour(isMouseOver() ? colour::outline : colour::wellLine);
                g.drawRoundedRectangle(area.reduced(0.5f), 3.0f, 1.0f);
                g.setFont(bodyFont(12.5f));
                g.setColour(colour::ink);
                g.drawText(value ? value() : juce::String(), area.reduced(10.0f, 0.0f).withTrimmedRight(16.0f), juce::Justification::centredLeft, true);
                juce::Path arrow;
                const float x = area.getRight() - 14.0f, y = area.getCentreY();
                arrow.addTriangle(x - 4.0f, y - 2.0f, x + 4.0f, y - 2.0f, x, y + 3.0f);
                g.setColour(colour::orange);
                g.fillPath(arrow);
            }

            void mouseUp(const juce::MouseEvent& e) override
            {
                if (! e.mouseWasClicked() || ! items)
                    return;
                juce::PopupMenu menu;
                const auto list = items();
                const auto now = value ? value() : juce::String();
                for (int i = 0; i < list.size(); ++i)
                    menu.addItem(i + 1, list[i], true, list[i] == now);
                menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMinimumWidth(getWidth()),
                                   [safe = juce::Component::SafePointer<Picker>(this)](int r)
                {
                    if (safe != nullptr && r > 0 && safe->choose)
                        safe->choose(r - 1);
                });
            }
        };

        /** The MIDI inputs as one list: a tick and a name a row; a click switches that input on or off. */
        class InputList final : public juce::Component
        {
        public:
            struct Input
            {
                juce::String name, id;
            };
            std::vector<Input> rows;
            std::function<bool(const juce::String&)> isOn;
            std::function<void(const juce::String&)> toggle;
            static constexpr int rowPitch = 24;

            InputList() { setRepaintsOnMouseActivity(true); }

            void paint(juce::Graphics& g) override
            {
                const int hover = isMouseOver() ? getMouseXYRelative().y / rowPitch : -1;
                for (int i = 0; i < static_cast<int>(rows.size()); ++i)
                {
                    const auto r = juce::Rectangle<int>(0, i * rowPitch, getWidth(), rowPitch);
                    if (i == hover)
                    {
                        g.setColour(colour::ink.withAlpha(0.06f));
                        g.fillRect(r);
                    }
                    if (i > 0)
                    {
                        g.setColour(colour::wellLine);
                        g.fillRect(r.getX(), r.getY(), r.getWidth(), 1);
                    }
                    const bool on = isOn && isOn(rows[static_cast<std::size_t>(i)].id);
                    const auto tick = juce::Rectangle<float>(10.0f, static_cast<float>(r.getCentreY()) - 6.0f, 12.0f, 12.0f);
                    if (on)
                    {
                        g.setColour(colour::orange);
                        g.fillRoundedRectangle(tick, 2.0f);
                        juce::Path mark;
                        mark.startNewSubPath(tick.getX() + 2.8f, tick.getCentreY());
                        mark.lineTo(tick.getX() + 5.2f, tick.getBottom() - 3.0f);
                        mark.lineTo(tick.getRight() - 2.6f, tick.getY() + 3.0f);
                        g.setColour(colour::orangeInk);
                        g.strokePath(mark, juce::PathStrokeType(1.6f));
                    }
                    else
                    {
                        g.setColour(colour::outline);
                        g.drawRoundedRectangle(tick.reduced(0.5f), 2.0f, 1.0f);
                    }
                    g.setColour(colour::ink);
                    g.setFont(bodyFont(12.0f));
                    g.drawText(rows[static_cast<std::size_t>(i)].name, r.withTrimmedLeft(30).withTrimmedRight(8), juce::Justification::centredLeft, true);
                }
            }

            void mouseUp(const juce::MouseEvent& e) override
            {
                const int i = e.y / rowPitch;
                if (e.mouseWasClicked() && i >= 0 && i < static_cast<int>(rows.size()) && toggle)
                    toggle(rows[static_cast<std::size_t>(i)].id);
                repaint();
            }
            void mouseMove(const juce::MouseEvent&) override { repaint(); }
        };

        juce::String rateText(double rate) { return juce::String(juce::roundToInt(rate)) + " Hz"; }
        juce::String bufferText(int size, double rate)
        {
            return juce::String(size) + " samples" + (rate > 0.0 ? " (" + juce::String(1000.0 * size / rate, 1) + " ms)" : juce::String());
        }
    }

    // =============================================================================================================

    /** The options in pages: their list down the left, one page at a time beside it (as the 808's, D335). */
    class OptionsPanel final : public juce::Component, private juce::ChangeListener, private juce::Timer
    {
    public:
        enum Page { audioPage, midiPage, hostPage, panelPage, exportPage };

        OptionsPanel(OptionsHooks h, juce::AudioDeviceManager* dm) : hooks(std::move(h)), devices(dm)
        {
            setWantsKeyboardFocus(false);
            if (devices != nullptr)
                pages = { audioPage, midiPage, panelPage, exportPage };
            else
                pages = { hostPage, panelPage, exportPage };
            page = pages.front();

            if (devices != nullptr)
                buildDevices();

            // The accuracy (D349): two keys, the chosen one lit; on the Audio page, the Host page in a plugin.
            for (int level = 0; level < static_cast<int>(accuracyKeys.size()); ++level)
            {
                auto& key = accuracyKeys[static_cast<std::size_t>(level)];
                key = std::make_unique<Key>(level == 0 ? "Best" : "Light", false);
                key->setComponentID("accuracy" + juce::String(level));
                key->isOn = [this, level] { return (hooks.accuracy ? hooks.accuracy() : 0) == level; };
                key->onClick = [this, level]
                {
                    if (hooks.chooseAccuracy)
                        hooks.chooseAccuracy(level);
                    repaint();
                };
                addChildComponent(*key);
            }

            windowSize.value = [this] { return juce::String(hooks.windowSize ? hooks.windowSize() : 100) + " %"; };
            windowSize.items = []
            {
                juce::StringArray names;
                for (const auto z : windowSizes)
                    names.add(juce::String(z) + " %");
                return names;
            };
            windowSize.choose = [this](int i)
            {
                if (hooks.chooseWindowSize)
                    hooks.chooseWindowSize(windowSizes[static_cast<std::size_t>(i)]);
                repaint();
            };
            addChildComponent(windowSize);

            theme.value = [this] { return themeNames()[hooks.theme ? hooks.theme() : 0]; };
            theme.items = [] { return themeNames(); };
            theme.choose = [this](int i)
            {
                if (hooks.chooseTheme)
                    hooks.chooseTheme(i);
                repaint();
            };
            addChildComponent(theme);

            rebuildInputs();
            startTimer(500);
        }

        ~OptionsPanel() override
        {
            stopTimer();
            if (devices != nullptr)
                devices->removeChangeListener(this);
        }

        /** The page shown, by its place in the list. */
        void showPage(int index)
        {
            index = juce::jlimit(0, pageCount() - 1, index);
            page = pages[static_cast<std::size_t>(index)];
            if (onPageChanged)
                onPageChanged(index);
            layOut();
            repaint();
        }
        [[nodiscard]] int pageIndex() const
        {
            for (std::size_t i = 0; i < pages.size(); ++i)
                if (pages[i] == page)
                    return static_cast<int>(i);
            return 0;
        }
        [[nodiscard]] int pageCount() const { return static_cast<int>(pages.size()); }
        [[nodiscard]] juce::String pageName(int index) const { return nameOf(pages[static_cast<std::size_t>(index)]); }
        std::function<void(int)> onPageChanged;

        void paint(juce::Graphics& g) override
        {
            g.fillAll(colour::face);

            // The pages' list: a lamp and a name each, the page shown on a key of its own, its lamp lit.
            g.setColour(colour::stencil);
            g.fillRect(0, 0, sideWidth, getHeight());
            g.setColour(juce::Colour(0xff111111));
            g.fillRect(sideWidth - 1, 0, 1, getHeight());
            for (int i = 0; i < pageCount(); ++i)
            {
                const auto r = pageArea(i).toFloat();
                const bool shown = i == pageIndex();
                if (shown)
                {
                    g.setColour(juce::Colours::black);
                    g.fillRoundedRectangle(r.translated(0.0f, 1.0f), 2.0f);
                    g.setColour(colour::key);
                    g.fillRoundedRectangle(r, 2.0f);
                }
                else if (i == hoverPage)
                {
                    g.setColour(colour::ink.withAlpha(0.06f));
                    g.fillRoundedRectangle(r, 2.0f);
                }
                drawLamp(g, { r.getX() + 13.0f, r.getCentreY() }, shown);
                g.setColour(shown ? colour::keyInk : i == hoverPage ? colour::screenCaption : colour::screenLabel);
                g.setFont(labelFont(11.0f));
                g.drawText(pageName(i).toUpperCase(), r.withTrimmedLeft(26.0f), juce::Justification::centredLeft, false);
            }

            // The page: its name over a thin line, and each row's label.
            const int x = sideWidth + pad, w = getWidth() - x - pad;
            g.setColour(colour::screenCaption);
            g.setFont(labelFont(12.0f).withExtraKerningFactor(0.12f));
            g.drawText(nameOf(page).toUpperCase(), juce::Rectangle<int>(x, topPad, w, headHeight), juce::Justification::centredLeft, false);
            g.setColour(colour::outline);
            g.fillRect(x, topPad + headHeight, w, 1);
            for (const auto& [text, y] : labels)
            {
                g.setColour(colour::screenDim);
                g.setFont(labelFont(10.5f));
                g.drawText(text.toUpperCase(), juce::Rectangle<int>(x, y, labelWidth, rowHeight), juce::Justification::centredLeft, false);
            }
            if (page == audioPage)
            {
                // the latency the app adds and how busy the audio is, under the accuracy
                g.setColour(colour::orange);
                g.setFont(monoFont(11.5f));
                g.drawText(readout(), juce::Rectangle<int>(x + labelWidth, readoutY, w - labelWidth - 120, rowHeight), juce::Justification::centredLeft, false);
                g.setColour(colour::screenHint);
                g.setFont(bodyFont(11.5f));
                g.drawFittedText(status, juce::Rectangle<int>(x + labelWidth, readoutY + rowHeight + 4, w - labelWidth, 36), juce::Justification::topLeft, 2);
            }
            if (page == midiPage)
            {
                if (inputList.rows.empty())
                {
                    g.setColour(colour::screenHint);
                    g.setFont(bodyFont(12.0f));
                    g.drawText("No MIDI input is connected.", juce::Rectangle<int>(x + labelWidth, inputsY, w - labelWidth, rowHeight), juce::Justification::centredLeft, false);
                }
                else
                {
                    const auto box = inputView.getBounds().toFloat().expanded(1.0f);
                    g.setColour(colour::well);
                    g.fillRoundedRectangle(box, 3.0f);
                    g.setColour(colour::wellLine);
                    g.drawRoundedRectangle(box.reduced(0.5f), 3.0f, 1.0f);
                }
            }
            if (hint.isNotEmpty())
            {
                g.setColour(colour::screenHint);
                g.setFont(bodyFont(12.0f));
                g.drawFittedText(hint, juce::Rectangle<int>(x, hintY, w, 36), juce::Justification::topLeft, 2);
            }
        }

        void resized() override { layOut(); }

        void mouseMove(const juce::MouseEvent& e) override
        {
            const int over = pageAt(e.getPosition());
            if (over != hoverPage)
            {
                hoverPage = over;
                repaint(0, 0, sideWidth, getHeight());
            }
        }
        void mouseExit(const juce::MouseEvent&) override
        {
            hoverPage = -1;
            repaint(0, 0, sideWidth, getHeight());
        }
        void mouseDown(const juce::MouseEvent& e) override
        {
            if (const int i = pageAt(e.getPosition()); i >= 0)
                showPage(i);
        }

    private:
        [[nodiscard]] static juce::String nameOf(Page p)
        {
            switch (p)
            {
                case audioPage: return "Audio";
                case midiPage: return "MIDI";
                case hostPage: return "Host";
                case panelPage: return "Panel";
                case exportPage: return "Export";
            }
            return {};
        }
        [[nodiscard]] juce::Rectangle<int> pageArea(int i) const { return { sidePad, 12 + i * pagePitch, sideWidth - 2 * sidePad, pageHeight }; }
        [[nodiscard]] int pageAt(juce::Point<int> p) const
        {
            for (int i = 0; i < pageCount(); ++i)
                if (pageArea(i).contains(p))
                    return i;
            return -1;
        }

        void buildDevices()
        {
            driver.value = [this]
            {
                const auto name = devices->getCurrentAudioDeviceType();
                return name.isNotEmpty() ? name : juce::String("None");
            };
            driver.items = [this]
            {
                // Windows Audio, its Low Latency mode and ASIO when built with it, as the 808's (D311): not the
                // Exclusive mode, which takes the device from every other app, nor the old DirectSound.
                juce::StringArray names;
                for (auto* type : devices->getAvailableDeviceTypes())
                    if (const auto name = type->getTypeName(); ! name.containsIgnoreCase("Exclusive") && ! name.containsIgnoreCase("DirectSound"))
                        names.add(name);
                return names;
            };
            driver.choose = [this](int i)
            {
                devices->setCurrentAudioDeviceType(driver.items()[i], true);
                changed();
            };
            output.value = [this]
            {
                auto* device = devices->getCurrentAudioDevice();
                return device != nullptr ? device->getName() : juce::String("None");
            };
            output.items = [this]
            {
                juce::StringArray names;
                if (auto* type = devices->getCurrentDeviceTypeObject())
                {
                    type->scanForDevices();
                    names = type->getDeviceNames(false);
                }
                return names;
            };
            output.choose = [this](int i) { applySetup([name = output.items()[i]](auto& s) { s.outputDeviceName = name; s.inputDeviceName = {}; }); };
            rate.value = [this]
            {
                auto* device = devices->getCurrentAudioDevice();
                return device != nullptr ? rateText(device->getCurrentSampleRate()) : juce::String("-");
            };
            rate.items = [this]
            {
                juce::StringArray names;
                if (auto* device = devices->getCurrentAudioDevice())
                    for (const auto r : device->getAvailableSampleRates())
                        names.add(rateText(r));
                return names;
            };
            rate.choose = [this](int i)
            {
                if (auto* device = devices->getCurrentAudioDevice())
                    applySetup([r = device->getAvailableSampleRates()[i]](auto& s) { s.sampleRate = r; });
            };
            buffer.value = [this]
            {
                auto* device = devices->getCurrentAudioDevice();
                return device != nullptr ? bufferText(device->getCurrentBufferSizeSamples(), device->getCurrentSampleRate()) : juce::String("-");
            };
            buffer.items = [this]
            {
                juce::StringArray names;
                if (auto* device = devices->getCurrentAudioDevice())
                    for (const auto b : device->getAvailableBufferSizes())
                        names.add(bufferText(b, device->getCurrentSampleRate()));
                return names;
            };
            buffer.choose = [this](int i)
            {
                if (auto* device = devices->getCurrentAudioDevice())
                    applySetup([b = device->getAvailableBufferSizes()[i]](auto& s) { s.bufferSize = b; });
            };
            for (auto* p : { &driver, &output, &rate, &buffer })
                addChildComponent(*p);

            // A driver with a panel of its own (ASIO) sets its buffer there; the device is opened again after it.
            controlPanel.onClick = [this]
            {
                if (auto* device = devices->getCurrentAudioDevice(); device != nullptr && device->showControlPanel())
                {
                    devices->closeAudioDevice();
                    devices->restartLastAudioDevice();
                    changed();
                }
            };
            addChildComponent(controlPanel);
            testTone.onClick = [this] { devices->playTestSound(); };
            addChildComponent(testTone);

            inputList.isOn = [this](const juce::String& id) { return devices->isMidiInputDeviceEnabled(id); };
            inputList.toggle = [this](const juce::String& id)
            {
                devices->setMidiInputDeviceEnabled(id, ! devices->isMidiInputDeviceEnabled(id));
                changed();
            };
            inputView.setViewedComponent(&inputList, false);
            inputView.setScrollBarsShown(true, false);
            inputView.setScrollBarThickness(8);
            addChildComponent(inputView);

            // The MIDI output the 303's clock and notes go to (JUCE's player sends to the default output).
            midiOut.value = [this]
            {
                const auto id = devices->getDefaultMidiOutputIdentifier();
                for (const auto& d : juce::MidiOutput::getAvailableDevices())
                    if (d.identifier == id)
                        return d.name;
                return juce::String("None");
            };
            midiOut.items = []
            {
                juce::StringArray names { "None" };
                for (const auto& d : juce::MidiOutput::getAvailableDevices())
                    names.add(d.name);
                return names;
            };
            midiOut.choose = [this](int i)
            {
                const auto list = juce::MidiOutput::getAvailableDevices();
                devices->setDefaultMidiOutputDevice(i == 0 || i > list.size() ? juce::String() : list[i - 1].identifier);
                changed();
            };
            addChildComponent(midiOut);
            devices->addChangeListener(this);
        }

        void layOut()
        {
            labels.clear();
            hint.clear();
            for (auto* c : getChildren())
                c->setVisible(false);
            const int x = sideWidth + pad, w = getWidth() - x - pad, cw = w - labelWidth;
            int y = topPad + headHeight + 12;
            const auto row = [&](juce::Component& c, const juce::String& label, int width = -1)
            {
                labels.push_back({ label, y });
                c.setBounds(x + labelWidth, y, width < 0 ? cw : width, rowHeight);
                c.setVisible(true);
                y += rowHeight + rowGap;
            };
            const auto accuracyRow = [&]
            {
                labels.push_back({ "Accuracy", y });
                for (std::size_t i = 0; i < accuracyKeys.size(); ++i)
                {
                    accuracyKeys[i]->setBounds(x + labelWidth + static_cast<int>(i) * (accuracyKeyWidth + 6), y + 1, accuracyKeyWidth, rowHeight - 2);
                    accuracyKeys[i]->setVisible(true);
                }
                y += rowHeight + rowGap;
            };
            switch (page)
            {
                case audioPage:
                {
                    row(driver, "Driver");
                    row(output, "Output");
                    row(rate, "Sample rate");
                    const bool ownPanel = devices->getCurrentAudioDevice() != nullptr && devices->getCurrentAudioDevice()->hasControlPanel();
                    if (ownPanel)
                    {
                        labels.push_back({ "Buffer", y });
                        controlPanel.setBounds(x + labelWidth, y + 1, 150, rowHeight - 2);
                        controlPanel.setVisible(true);
                        y += rowHeight + rowGap;
                    }
                    else
                        row(buffer, "Buffer");
                    accuracyRow();
                    readoutY = y;
                    testTone.setBounds(x + w - 100, y + 1, 100, rowHeight - 2);
                    testTone.setVisible(true);
                    break;
                }
                case midiPage:
                {
                    inputsY = y;
                    labels.push_back({ "Inputs", y });
                    const int count = static_cast<int>(inputList.rows.size());
                    const int shownRows = juce::jlimit(1, maxInputRows, count);
                    inputList.setSize(cw - (count > maxInputRows ? 10 : 0), count * InputList::rowPitch);
                    if (count > 0)
                    {
                        inputView.setBounds(x + labelWidth, y, cw, shownRows * InputList::rowPitch);
                        inputView.setVisible(true);
                        y += shownRows * InputList::rowPitch + rowGap + 4;
                    }
                    else
                        y += rowHeight + rowGap;
                    row(midiOut, "Output");
                    break;
                }
                case hostPage:
                    accuracyRow();
                    break;
                case panelPage:
                    row(windowSize, "Window size", 150);
                    row(theme, "Theme", 150);
                    break;
                case exportPage:
                    hint = "The WAV export comes here.";
                    hintY = y + 6;
                    break;
            }
        }

        void rebuildInputs()
        {
            if (devices == nullptr)
                return;
            const auto list = juce::MidiInput::getAvailableDevices();
            knownInputs = list.size();
            inputList.rows.clear();
            for (const auto& d : list)
                inputList.rows.push_back({ d.name, d.identifier });
            layOut();
            repaint();
        }

        template <typename Edit>
        void applySetup(Edit edit)
        {
            auto setup = devices->getAudioDeviceSetup();
            edit(setup);
            const auto error = devices->setAudioDeviceSetup(setup, true);
            status = error.isNotEmpty() ? "Could not open it: " + error : juce::String();
            changed();
        }

        void changed()
        {
            saveStandaloneDeviceState();
            layOut();
            repaint();
            inputList.repaint();
        }

        juce::String readout() const
        {
            auto* device = devices->getCurrentAudioDevice();
            if (device == nullptr)
                return "No audio device open";
            const double r = device->getCurrentSampleRate();
            const double ms = r > 0.0 ? 1000.0 * (device->getOutputLatencyInSamples() + device->getCurrentBufferSizeSamples()) / r : 0.0;
            return "Latency " + juce::String(ms, 1) + " ms   CPU " + juce::String(juce::roundToInt(devices->getCpuUsage() * 100.0)) + " %";
        }

        void changeListenerCallback(juce::ChangeBroadcaster*) override
        {
            layOut();
            repaint();
            inputList.repaint();
        }

        void timerCallback() override
        {
            if (devices == nullptr)
                return;
            if (juce::MidiInput::getAvailableDevices().size() != knownInputs)
                rebuildInputs();
            if (page == audioPage)
                repaint(sideWidth, readoutY, getWidth() - sideWidth, rowHeight);
        }

        static constexpr int maxInputRows = 8, accuracyKeyWidth = 96;

        OptionsHooks hooks;
        juce::AudioDeviceManager* devices;
        std::vector<Page> pages;
        Page page = audioPage;
        int hoverPage = -1;
        Picker driver, output, rate, buffer, midiOut, windowSize, theme;
        Key controlPanel { "Control panel", false }, testTone { "Test tone", false };
        std::array<std::unique_ptr<Key>, 2> accuracyKeys;
        InputList inputList;
        juce::Viewport inputView;
        int knownInputs = -1;
        std::vector<std::pair<juce::String, int>> labels;
        juce::String hint, status;
        int readoutY = 0, inputsY = 0, hintY = 0;
    };

    // =============================================================================================================

    OptionsWindow::OptionsWindow(OptionsHooks hooks, juce::AudioDeviceManager* devices, juce::PropertiesFile::Options settingsFile)
        : juce::DocumentWindow("CoolSynth 303 Options", colour::face, juce::DocumentWindow::closeButton, false),
          settings(std::move(settingsFile))
    {
        setLookAndFeel(&WindowLook::shared());
        setTitleBarHeight(28);
        panel = std::make_unique<OptionsPanel>(std::move(hooks), devices);
        panel->setSize(panelWidth, panelHeight);
        // It opens on the page last shown, where it was last put (else in the middle of the screen).
        juce::PropertiesFile kept(settings);
        panel->showPage(kept.getIntValue("optionsPage", 0));
        panel->onPageChanged = [this](int index)
        {
            // A file opened for each change, as the processor's accuracy is kept: neither writes over the other.
            juce::PropertiesFile file(settings);
            file.setValue("optionsPage", index);
            file.saveIfNeeded();
        };
        setContentNonOwned(panel.get(), true);
        setContentComponentSize(panelWidth, panelHeight);
        setResizable(false, false);
        const int x = kept.getIntValue("optionsX", -100000), y = kept.getIntValue("optionsY", -100000);
        if (x != -100000 && y != -100000)
            setBoundsConstrained({ x, y, getWidth(), getHeight() });
        else
            centreWithSize(getWidth(), getHeight());
        setWantsKeyboardFocus(true);
        addToDesktop(getDesktopWindowStyleFlags()); // with this window's flags: no taskbar button
    }

    OptionsWindow::~OptionsWindow()
    {
        // Its place, kept as it closes (not at every step of a drag).
        if (isOnDesktop())
        {
            juce::PropertiesFile file(settings);
            file.setValue("optionsX", getX());
            file.setValue("optionsY", getY());
            file.saveIfNeeded();
        }
        clearContentComponent();
        setLookAndFeel(nullptr);
    }

    void OptionsWindow::closeButtonPressed()
    {
        if (onClose)
            onClose();
    }

    bool OptionsWindow::keyPressed(const juce::KeyPress& key)
    {
        if (key == juce::KeyPress::escapeKey)
        {
            closeButtonPressed();
            return true;
        }
        return juce::DocumentWindow::keyPressed(key);
    }

    void OptionsWindow::keepAbove(juce::Component& owner)
    {
       #if JUCE_WINDOWS
        // JUCE cannot give a window an owner (forum.juce.com/t/fr-floating-child-windows/69136): Windows' own call
        // does. An owned window stays above its owner and goes with it, but not above other apps (as the 808's).
        auto* top = owner.getTopLevelComponent();
        auto* mine = getPeer();
        auto* theirs = top != nullptr ? top->getPeer() : nullptr;
        if (mine != nullptr && theirs != nullptr)
        {
            constexpr int ownerIndex = -8; // GWLP_HWNDPARENT: the owner of a top-level window
            SetWindowLongPtrW(mine->getNativeHandle(), ownerIndex, reinterpret_cast<std::intptr_t>(theirs->getNativeHandle()));
            constexpr unsigned int noSize = 0x1, noMove = 0x2, noActivate = 0x10;
            SetWindowPos(mine->getNativeHandle(), reinterpret_cast<void*>(static_cast<std::intptr_t>(-2)) /* HWND_NOTOPMOST */, 0, 0, 0, 0, noSize | noMove | noActivate);
        }
       #else
        juce::ignoreUnused(owner);
       #endif
    }

    int OptionsWindow::getDesktopWindowStyleFlags() const
    {
        // no button of its own on the taskbar: it belongs to the 303's window
        return juce::DocumentWindow::getDesktopWindowStyleFlags() & ~juce::ComponentPeer::windowAppearsOnTaskbar;
    }

    std::unique_ptr<juce::Component> makeOptionsPanelForTesting(OptionsHooks hooks, juce::AudioDeviceManager* devices)
    {
        auto p = std::make_unique<OptionsPanel>(std::move(hooks), devices);
        p->setSize(panelWidth, panelHeight);
        return p;
    }

    int showOptionsPageForTesting(juce::Component& panel, int page)
    {
        auto* p = dynamic_cast<OptionsPanel*>(&panel);
        if (p == nullptr)
            return 0;
        if (page >= 0 && page < p->pageCount())
            p->showPage(page);
        return p->pageCount();
    }

    juce::String optionsPageNameForTesting(juce::Component& panel)
    {
        auto* p = dynamic_cast<OptionsPanel*>(&panel);
        return p != nullptr ? p->pageName(p->pageIndex()) : juce::String();
    }
}
