#pragma once

#include <functional>
#include <memory>

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "AudioExport.h"

/*
    CoolSynth 303's own options (D349, after CoolSynth 808's D311 and D335), in a window of their own, opened by the
    top bar's Options key, in pages listed down its left. Standalone: Audio (driver, output, sample rate, buffer, the
    accuracy), MIDI (the inputs, the output; the 303 always follows and sends MIDI clock, so no clock switches), Panel
    (the window's size), Export. In a plugin: Host (the accuracy; the 303 always follows the host's transport), Panel,
    Export. It stays above the 303's window, remembers its place and the page last shown, and closes on its close
    key, Escape or the Options key. Drawn only through Theme.h, so a new look for the app restyles it too.

    Export (D355, as CoolSynth 808's export dialog, D331): what plays, how many times, the end, the files and the
    rate, each a row of keys; Export asks where, renders on a thread of its own while the app plays on, and says what
    it wrote. Closing the window cancels it, and a cancelled export leaves no file.
*/
namespace cs303::ui
{
    class OptionsPanel;

    /** What the options act on, handed in by the editor. */
    struct OptionsHooks
    {
        // The circuits' accuracy (D349): 0 Best, 1 Light.
        std::function<int()> accuracy;
        std::function<void(int)> chooseAccuracy;
        // The window's size, in percent of the panel's 1280 by 800.
        std::function<int()> windowSize;
        std::function<int()> theme;            // 0 Graphite, 1 Silver, 2 Steel, 3 Olive (D352)
        std::function<void(int)> chooseTheme;
        std::function<void(int)> chooseWindowSize;
        // The WAV export (D355): the pattern the panel shows ("Pattern 01A"); the job for it or the chain, the project
        // as it is; the app's rate (the host's in a plugin); the page's choices kept for next time (none: not kept).
        std::function<juce::String()> patternShown;
        std::function<ExportJob(bool chain)> exportJob;
        std::function<double()> appRate;
        std::function<juce::var(const juce::String&)> kept;
        std::function<void(const juce::NamedValueSet&)> keep;
    };

    class OptionsWindow final : public juce::DocumentWindow
    {
    public:
        /** `devices` is the standalone app's device manager, or null in a plugin (those pages left out). The window's
            place and its page are kept in the file `settings` names. */
        OptionsWindow(OptionsHooks hooks, juce::AudioDeviceManager* devices, juce::PropertiesFile::Options settings);
        ~OptionsWindow() override;

        std::function<void()> onClose;

        void closeButtonPressed() override;
        bool keyPressed(const juce::KeyPress& key) override;
        /** Kept above `owner`'s window, not above other apps: on Windows the owner of its native window. */
        void keepAbove(juce::Component& owner);
        int getDesktopWindowStyleFlags() const override;

    private:
        std::unique_ptr<OptionsPanel> panel;
        juce::PropertiesFile::Options settings;
    };

    /** The options as the window shows them, alone, for the tests and snapshots. */
    std::unique_ptr<juce::Component> makeOptionsPanelForTesting(OptionsHooks hooks, juce::AudioDeviceManager* devices);
    /** Shows one of the panel's pages by its place in the list; returns how many pages it has. */
    int showOptionsPageForTesting(juce::Component& panel, int page);
    /** The name of the page shown. */
    juce::String optionsPageNameForTesting(juce::Component& panel);
    /** The export's job as the page's choices make it. */
    ExportJob exportJobForTesting(juce::Component& panel);
    /** Exports to a file without asking where. */
    void exportToForTesting(juce::Component& panel, const juce::File& mixFile);
    [[nodiscard]] bool exportBusyForTesting(juce::Component& panel);
    /** Waits up to `ms` for the export's thread, then ends it as the page's timer does; false if still busy. */
    bool finishExportForTesting(juce::Component& panel, int ms);
    [[nodiscard]] juce::String exportStatusForTesting(juce::Component& panel);
}
