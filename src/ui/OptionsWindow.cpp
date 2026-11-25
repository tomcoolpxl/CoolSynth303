#include "ui/OptionsWindow.h"

#include <array>
#include <atomic>
#include <cmath>
#include <thread>
#include <vector>

#include "StandaloneDevices.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "ui/Hints.h"
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
        constexpr int panelWidth = 600, panelHeight = 372 + 26, footHeight = 26, sideWidth = 148, pad = 22, labelWidth = 104, rowHeight = 28, rowGap = 8,
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
        juce::String kiloHertz(double rate)
        {
            const double k = rate / 1000.0;
            return (std::abs(k - std::round(k)) < 0.01 ? juce::String(juce::roundToInt(k)) : juce::String(k, 1)) + " kHz";
        }
        juce::String bufferText(int size, double rate)
        {
            return juce::String(size) + " samples" + (rate > 0.0 ? " (" + juce::String(1000.0 * size / rate, 1) + " ms)" : juce::String());
        }
    }

    // =============================================================================================================

    /** The options in pages: their list down the left, one page at a time beside it (as the 808's, D335). */
    class OptionsPanel final : public juce::Component, public HintSource, private juce::ChangeListener, private juce::Timer
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

            // the hint line at the foot (D353): what the row under the mouse does
            hintLine = std::make_unique<HintLine>(true);
            addAndMakeVisible(*hintLine);
            hintWatcher = std::make_unique<HintWatcher>(*this, *hintLine);
            addMouseListener(hintWatcher.get(), true);

            // The accuracy (D349): two keys, the chosen one lit; on the Audio page, the Host page in a plugin.
            for (int level = 0; level < static_cast<int>(accuracyKeys.size()); ++level)
            {
                auto& key = accuracyKeys[static_cast<std::size_t>(level)];
                key = std::make_unique<Key>(level == 0 ? "Best" : "Light", false);
                key->setComponentID("accuracy" + juce::String(level));
                key->setHelpText(level == 0 ? "Best: the circuit solved as closely as it goes; the most CPU."
                                            : "Light: the voice at the device's rate with a looser solve; about half the CPU, the top a little duller.");
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
            windowSize.setHelpText("The window's size, in percent of the panel's own.");
            addChildComponent(windowSize);

            theme.value = [this] { return themeNames()[hooks.theme ? hooks.theme() : 0]; };
            theme.items = [] { return themeNames(); };
            theme.choose = [this](int i)
            {
                if (hooks.chooseTheme)
                    hooks.chooseTheme(i);
                repaint();
            };
            theme.setHelpText("The panel's colours: Graphite, Silver after the TB-303, the 808's Steel and Olive.");
            addChildComponent(theme);

            buildExport();
            rebuildInputs();
            startTimer(500);
        }

        ~OptionsPanel() override
        {
            // closing the window cancels an export, and waits for its thread (which leaves no file)
            exportTick.stopTimer();
            stopExport();
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
            g.setColour(colour::face.darker(0.4f)); // a shade under the dialog's glass, in every theme
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
            if (page == exportPage)
            {
                // what comes out, under the rows
                const auto j = exportJob();
                juce::String out = "24-bit WAV at " + kiloHertz(j.sampleRate) + ", " + juce::String(static_cast<double>(loopSamples(j)) / j.sampleRate, 1) + " s";
                if (j.end == ExportEnd::ring)
                    out << " and the ring-out";
                if (j.end == ExportEnd::loop)
                    out << ", the ring-out in its start";
                g.setColour(colour::screenHint);
                g.setFont(bodyFont(12.0f));
                shapedText(g, out, juce::Rectangle<int>(x + labelWidth, summaryY, w - labelWidth, 20), juce::Justification::centredLeft, true);

                // beside the key: the progress while it renders; under it, what was written
                if (exportBusy())
                {
                    const int left = cancelKey.getRight() + 12;
                    const auto bar = juce::Rectangle<int>(left, exportY + rowHeight / 2 - 4, x + w - left, 8).toFloat();
                    g.setColour(colour::well);
                    g.fillRoundedRectangle(bar, 4.0f);
                    g.setColour(colour::wellLine);
                    g.drawRoundedRectangle(bar.reduced(0.5f), 4.0f, 1.0f);
                    g.setColour(colour::orange);
                    g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * juce::jlimit(0.0f, 1.0f, progress.load())), 4.0f);
                }
                else if (exportStatus.isNotEmpty())
                {
                    // a line each: what, then the folder (cut at its end should it be too long)
                    g.setColour(exportOk ? colour::orange : colour::screenHint);
                    g.setFont(bodyFont(11.5f));
                    const auto lines = juce::StringArray::fromLines(exportStatus);
                    for (int i = 0; i < lines.size(); ++i)
                        shapedText(g, lines[i], juce::Rectangle<int>(x + labelWidth, exportY + rowHeight + 8 + i * 18, w - labelWidth, 18), juce::Justification::centredLeft, true);
                }
            }
            // the foot: a strip of its own for the hint line, a line over it
            const auto foot = getLocalBounds().removeFromBottom(footHeight);
            g.setColour(colour::face);
            g.fillRect(foot);
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.fillRect(foot.withHeight(1));
        }

        void resized() override
        {
            layOut();
            hintLine->setBounds(sidePad + 4, getHeight() - footHeight + 3, getWidth() - 2 * sidePad - 8, footHeight - 6);
        }

        [[nodiscard]] juce::String hintAt(juce::Point<int> p) const override
        {
            const int i = pageAt(p);
            if (i < 0)
                return {};
            switch (pages[static_cast<std::size_t>(i)])
            {
                case audioPage: return "Audio: the device, its rate and buffer, the accuracy, a test tone.";
                case midiPage: return "MIDI: the inputs that play the 303, and an output for its notes.";
                case hostPage: return "Host: the accuracy, in a plugin; the devices are the host's.";
                case panelPage: return "Panel: the window's size and the theme.";
                case exportPage: return "Export: a WAV of the pattern shown or the chain.";
            }
            return {};
        }

        // ---------------------------------------------------------------- the WAV export (D355)
        /** The job as the page's choices make it, the project as it is now. */
        [[nodiscard]] ExportJob exportJob() const
        {
            auto j = hooks.exportJob ? hooks.exportJob(exportWhat == 1) : ExportJob {};
            j.chain = exportWhat == 1;
            j.times = timesChoices[static_cast<std::size_t>(exportTimes)];
            j.end = static_cast<ExportEnd>(exportEnd);
            j.dry = exportFiles == 1;
            j.sampleRate = exportRate == 1 ? 96000.0 : appRate();
            return j;
        }
        /** From the start of an export until its end is shown (the thread may have finished a moment before). */
        [[nodiscard]] bool exportBusy() const noexcept { return worker != nullptr; }
        [[nodiscard]] juce::String exportStatusText() const { return exportStatus; }

        void startExport(const juce::File& mixFile)
        {
            stopExport();
            cancel.store(false);
            workerDone.store(false);
            progress.store(0.0f);
            exportOk = false;
            exportStatus.clear();
            exportTo = mixFile;
            exportWritten.clear();
            // The job taken here, on the message thread; rendered and written on the worker.
            worker = std::make_unique<std::thread>([this, j = exportJob(), mixFile]
            {
                auto result = std::make_unique<ExportResult>();
                bool fine = renderExport(j, *result, progress, cancel);
                juce::Array<juce::File> files;
                const auto write = [&](const juce::File& f, const juce::AudioBuffer<float>& audio)
                {
                    if (! fine || cancel.load())
                        return;
                    files.add(f);
                    fine = writeWav24(f, audio, j.sampleRate);
                };
                write(mixFile, result->mix);
                if (j.dry)
                    write(dryFile(mixFile), result->dry);
                const bool done = fine && ! cancel.load();
                if (! done)
                    for (const auto& f : files)
                        (void) f.deleteFile(); // cancelled or failed: nothing half written left behind
                for (const auto& f : files)
                    if (done)
                        exportWritten.add(f.getFileName());
                exportOk = done;
                workerDone.store(true, std::memory_order_release);
            });
            exportTick.startTimerHz(30);
            layOut();
            repaint();
        }

        /** The export's end, on the message thread: its thread joined, what it wrote said. */
        void finishExport()
        {
            exportTick.stopTimer();
            const bool cancelled = cancel.load();
            stopExport();
            if (exportOk)
                exportStatus = "Written: " + exportWritten.joinIntoString(" and ") + "\nin " + exportTo.getParentDirectory().getFullPathName();
            else
                exportStatus = cancelled ? juce::String("Cancelled; nothing written.")
                                         : "Could not write " + exportTo.getFileName() + "\nin " + exportTo.getParentDirectory().getFullPathName();
            layOut();
            repaint();
        }

        /** Waits for the export's thread (the tests'); true once it has ended. */
        bool waitForExport(int ms)
        {
            for (int waited = 0; worker != nullptr && ! workerDone.load(std::memory_order_acquire) && waited < ms; waited += 10)
                juce::Thread::sleep(10);
            if (worker != nullptr && workerDone.load(std::memory_order_acquire))
                finishExport();
            return worker == nullptr;
        }

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
            driver.setHelpText("The audio driver: Windows Audio, or ASIO for the lowest latency.");
            output.setHelpText("The audio output the 303 plays through.");
            rate.setHelpText("The sample rate the audio device runs at.");
            buffer.setHelpText("The audio buffer: smaller answers faster, larger is safer from crackles.");
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
            controlPanel.setHelpText("The driver's own control panel (ASIO).");
            addChildComponent(controlPanel);
            testTone.onClick = [this] { devices->playTestSound(); };
            testTone.setHelpText("Play a short tone through the output, to hear that it works.");
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
            inputView.setHelpText("The MIDI inputs: tick those whose notes play the 303 and whose clock it follows.");
            addChildComponent(inputView);

            // The MIDI output the 303's clock and notes go to (JUCE's player sends to the default output).
            midiOut.setHelpText("A MIDI output for the sequencer's notes, to play another synth.");
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
            for (auto* c : getChildren())
                c->setVisible(c == hintLine.get()); // the hint line stays on every page
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
                {
                    // What, Times, End, Files, Rate: a row of keys each, as Accuracy's (D355)
                    static const char* const names[] = { "What", "Times", "End", "Files", "Rate" };
                    for (std::size_t r = 0; r < exportRows.size(); ++r)
                    {
                        labels.push_back({ names[r], y });
                        placeKeys(exportRows[r], x + labelWidth, y, cw);
                        y += rowHeight + rowGap;
                    }
                    summaryY = y;
                    exportY = y + 28;
                    for (auto* k : { &exportKey, &cancelKey })
                        k->setBounds(x + labelWidth, exportY + 1, exportKeyWidth, rowHeight - 2);
                    (exportBusy() ? cancelKey : exportKey).setVisible(true);
                    break;
                }
            }
        }

        /** A row of keys from `left`, within `width`: all as wide as Accuracy's keys, or as the widest name needs,
            or (where even that is too wide) each as its own name needs. */
        static void placeKeys(const std::vector<std::unique_ptr<Key>>& keys, int left, int y, int width)
        {
            constexpr int gap = 6, roomAround = 2 + 11 + 2 * 12; // the cap's edges, the lens, the margins
            const int n = static_cast<int>(keys.size());
            std::vector<int> need;
            int widest = 0, sum = 0;
            for (const auto& k : keys)
            {
                const auto text = (k->textOf ? k->textOf() : k->getName()).toUpperCase();
                need.push_back(static_cast<int>(std::ceil(shapedWidth(labelFont(10.5f), text))) + roomAround);
                widest = std::max(widest, need.back());
                sum += need.back();
            }
            const int room = width - (n - 1) * gap;
            const int even = n * std::max(widest, accuracyKeyWidth) <= room ? std::max(widest, accuracyKeyWidth) : n * widest <= room ? widest : 0;
            int at = left;
            for (int i = 0; i < n; ++i)
            {
                const int kw = even > 0 ? even : need[static_cast<std::size_t>(i)] + (room - sum) / n;
                keys[static_cast<std::size_t>(i)]->setBounds(at, y + 1, kw, rowHeight - 2);
                keys[static_cast<std::size_t>(i)]->setVisible(true);
                at += kw + gap;
            }
        }

        void buildExport()
        {
            // the choices kept from last time
            const auto keptChoice = [this](const char* name, int count)
            {
                return hooks.kept ? juce::jlimit(0, count - 1, static_cast<int>(hooks.kept(name))) : 0;
            };
            exportWhat = keptChoice("exportWhat", 2);
            exportTimes = keptChoice("exportTimes", static_cast<int>(timesChoices.size()));
            exportEnd = keptChoice("exportEnd", 3);
            exportFiles = keptChoice("exportFiles", 2);
            exportRate = keptChoice("exportRate", 2);

            const auto row = [this](std::size_t r, const char* id, const juce::StringArray& names, const juce::StringArray& helps, int& value)
            {
                for (int i = 0; i < names.size(); ++i)
                {
                    auto key = std::make_unique<Key>(names[i], false);
                    key->setName(names[i]);
                    key->setComponentID(id + juce::String(i));
                    key->setHelpText(helps[i]);
                    key->isOn = [&value, i] { return value == i; };
                    key->enabled = [this] { return ! exportBusy(); };
                    key->onClick = [this, &value, i]
                    {
                        if (exportBusy())
                            return;
                        value = i;
                        keepExportChoices();
                        repaint();
                    };
                    addChildComponent(*key);
                    exportRows[r].push_back(std::move(key));
                }
            };
            row(0, "exportWhat", { "Pattern 01A", "Chain" },
                { "The pattern the panel shows, played from its start.", "The chain, played from its start." }, exportWhat);
            juce::StringArray times, timesHelp;
            for (const int t : timesChoices)
            {
                times.add(juce::String(t));
                timesHelp.add(t == 1 ? juce::String("Played through once.") : "Played through " + juce::String(t) + " times.");
            }
            row(1, "exportTimes", times, timesHelp, exportTimes);
            row(2, "exportEnd", { "Cut", "Ring out", "Seamless loop" },
                { "Cut where the last time through ends.", "The ring-out after the end: the last note and the delay, until quiet.",
                  "A loop that joins itself: the end's ring-out in its start, as a loop plays it." }, exportEnd);
            row(3, "exportFiles", { "Mix", "Mix and dry" },
                { "The mix alone, as you hear it.", "The mix, and the dry voice beside it (Drive and Delay off) as \"Name - dry.wav\"." }, exportFiles);
            row(4, "exportRate", { "App rate", "96 kHz" },
                { "At the rate the app runs at now.", "At 96 kHz, the voice's own rate." }, exportRate);
            exportRows[0][0]->textOf = [this] { return hooks.patternShown ? hooks.patternShown() : juce::String("Pattern 01A"); };
            exportRows[4][0]->textOf = [this] { return kiloHertz(appRate()); };

            exportKey.setComponentID("exportGo");
            exportKey.setHelpText("Choose where to save, then render the WAV; the 303 plays on meanwhile.");
            exportKey.onClick = [this] { chooseExportFile(); };
            addChildComponent(exportKey);
            cancelKey.setComponentID("exportCancel");
            cancelKey.setHelpText("Stop the export; nothing is written.");
            cancelKey.onClick = [this] { cancel.store(true); };
            addChildComponent(cancelKey);
        }

        [[nodiscard]] double appRate() const
        {
            const double r = hooks.appRate ? hooks.appRate() : 48000.0;
            return r > 0.0 ? r : 48000.0;
        }

        void keepExportChoices()
        {
            if (! hooks.keep)
                return;
            juce::NamedValueSet values;
            values.set("exportWhat", exportWhat);
            values.set("exportTimes", exportTimes);
            values.set("exportEnd", exportEnd);
            values.set("exportFiles", exportFiles);
            values.set("exportRate", exportRate);
            hooks.keep(values);
        }

        void chooseExportFile()
        {
            if (exportBusy())
                return;
            auto folder = juce::File(hooks.kept ? hooks.kept("exportFolder").toString() : juce::String());
            if (! folder.isDirectory())
                folder = juce::File::getSpecialLocation(juce::File::userMusicDirectory);
            // the project's name as the file's
            auto name = juce::File::createLegalFileName(juce::String(exportJob().project.name).trim());
            if (name.isEmpty())
                name = "CoolSynth 303";
            chooser = std::make_unique<juce::FileChooser>("Export a WAV", folder.getChildFile(name + ".wav"), "*.wav");
            chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                                 [safe = juce::Component::SafePointer<OptionsPanel>(this)](const juce::FileChooser& fc)
            {
                if (safe == nullptr)
                    return;
                auto f = fc.getResult();
                if (f == juce::File())
                    return;
                if (! f.hasFileExtension("wav"))
                    f = f.withFileExtension("wav");
                if (safe->hooks.keep)
                {
                    juce::NamedValueSet values;
                    values.set("exportFolder", f.getParentDirectory().getFullPathName());
                    safe->hooks.keep(values);
                }
                safe->startExport(f);
            });
        }

        void tickExport()
        {
            if (workerDone.load(std::memory_order_acquire))
                finishExport();
            else if (page == exportPage)
                repaint(sideWidth, exportY, getWidth() - sideWidth, rowHeight);
        }

        void stopExport()
        {
            if (worker == nullptr)
                return;
            if (! workerDone.load())
                cancel.store(true);
            worker->join();
            worker.reset();
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

        static constexpr int maxInputRows = 8, accuracyKeyWidth = 96, exportKeyWidth = 110;

        OptionsHooks hooks;
        juce::AudioDeviceManager* devices;
        std::vector<Page> pages;
        Page page = audioPage;
        int hoverPage = -1;
        std::unique_ptr<HintLine> hintLine;
        std::unique_ptr<HintWatcher> hintWatcher;
        Picker driver, output, rate, buffer, midiOut, windowSize, theme;
        Key controlPanel { "Control panel", false }, testTone { "Test tone", false };
        std::array<std::unique_ptr<Key>, 2> accuracyKeys;
        InputList inputList;
        juce::Viewport inputView;
        int knownInputs = -1;
        std::vector<std::pair<juce::String, int>> labels;
        juce::String status;
        int readoutY = 0, inputsY = 0;

        // the WAV export (D355): the choices, as indices; its keys; the worker that renders and writes
        int exportWhat = 0, exportTimes = 0, exportEnd = 0, exportFiles = 0, exportRate = 0;
        static constexpr std::array<int, 5> timesChoices { 1, 2, 4, 8, 16 };
        std::array<std::vector<std::unique_ptr<Key>>, 5> exportRows;
        Key exportKey { "Export", false }, cancelKey { "Cancel", false };
        std::unique_ptr<juce::FileChooser> chooser;
        std::unique_ptr<std::thread> worker;
        std::atomic<bool> cancel { false }, workerDone { false };
        std::atomic<float> progress { 0.0f };
        bool exportOk = false;            // written by the worker before workerDone
        juce::StringArray exportWritten;  // likewise: the files' names
        juce::File exportTo;
        juce::String exportStatus;
        juce::TimedCallback exportTick { [this] { tickExport(); } };
        int summaryY = 0, exportY = 0;
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

    ExportJob exportJobForTesting(juce::Component& panel)
    {
        auto* p = dynamic_cast<OptionsPanel*>(&panel);
        return p != nullptr ? p->exportJob() : ExportJob {};
    }

    void exportToForTesting(juce::Component& panel, const juce::File& mixFile)
    {
        if (auto* p = dynamic_cast<OptionsPanel*>(&panel))
            p->startExport(mixFile);
    }

    bool exportBusyForTesting(juce::Component& panel)
    {
        auto* p = dynamic_cast<OptionsPanel*>(&panel);
        return p != nullptr && p->exportBusy();
    }

    bool finishExportForTesting(juce::Component& panel, int ms)
    {
        auto* p = dynamic_cast<OptionsPanel*>(&panel);
        return p == nullptr || p->waitForExport(ms);
    }

    juce::String exportStatusForTesting(juce::Component& panel)
    {
        auto* p = dynamic_cast<OptionsPanel*>(&panel);
        return p != nullptr ? p->exportStatusText() : juce::String();
    }
}
