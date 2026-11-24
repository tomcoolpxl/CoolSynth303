// The standalone app's own window (D349, as CoolSynth 808's D311): JUCE's bar in the 303's look and without JUCE's
// Options button; the top bar's Options key opens CoolSynth 303's own options instead. Otherwise JUCE's standalone as
// it was: the same settings file, its holder keeping the devices, the MIDI inputs going to the player, the plugin's
// state and the window's place.

#if JucePlugin_Build_Standalone

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#include "ui/Theme.h"
#include "ui/WindowLook.h"

namespace
{
    class Window final : public juce::StandaloneFilterWindow
    {
    public:
        Window(const juce::String& title, std::unique_ptr<juce::StandalonePluginHolder> holder)
            : juce::StandaloneFilterWindow(title, cs303::ui::colour::desk, std::move(holder))
        {
            setLookAndFeel(&cs303::ui::WindowLook::shared());
            // JUCE gives no way to leave its Options button out: it is hidden, found by its name (as the 808's).
            for (auto* child : getChildren())
                if (auto* button = dynamic_cast<juce::Button*>(child))
                    if (button->getName() == "Options" || button->getButtonText() == "Options")
                        button->setVisible(false);
        }

        ~Window() override { setLookAndFeel(nullptr); }

        void activeWindowStatusChanged() override
        {
            juce::StandaloneFilterWindow::activeWindowStatusChanged();
            repaint(0, 0, getWidth(), getTitleBarHeight()); // the title dims when the window is not the active one
        }
    };

    class App final : public juce::JUCEApplication
    {
    public:
        App()
        {
            // JUCE's own settings file (%APPDATA%\CoolSynth 303\CoolSynth 303.settings), so the devices chosen before
            // are kept.
            juce::PropertiesFile::Options options;
            options.applicationName = juce::CharPointer_UTF8(JucePlugin_Name);
            options.filenameSuffix = ".settings";
            options.osxLibrarySubFolder = "Application Support";
           #if JUCE_LINUX || JUCE_BSD
            options.folderName = "~/.config";
           #else
            options.folderName = "";
           #endif
            appProperties.setStorageParameters(options);
        }

        const juce::String getApplicationName() override { return juce::CharPointer_UTF8(JucePlugin_Name); }
        const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
        bool moreThanOneInstanceAllowed() override { return true; }
        void anotherInstanceStarted(const juce::String&) override {}

        void initialise(const juce::String&) override
        {
            if (juce::Desktop::getInstance().getDisplays().displays.isEmpty())
                return;
            // As JUCE's own app makes it: the MIDI inputs the settings switched on, none opened of itself.
            auto holder = std::make_unique<juce::StandalonePluginHolder>(appProperties.getUserSettings(), false, juce::String {}, nullptr,
                                                                          juce::Array<juce::StandalonePluginHolder::PluginInOuts> {}, false);
            mainWindow = std::make_unique<Window>(getApplicationName(), std::move(holder));
            mainWindow->setVisible(true);
        }

        void shutdown() override
        {
            mainWindow = nullptr;
            appProperties.saveIfNeeded();
        }

        void systemRequestedQuit() override
        {
            if (mainWindow != nullptr)
                mainWindow->pluginHolder->savePluginState();
            if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
            {
                juce::Timer::callAfterDelay(100, []
                {
                    if (auto* app = juce::JUCEApplicationBase::getInstance())
                        app->systemRequestedQuit();
                });
            }
            else
                quit();
        }

    private:
        juce::ApplicationProperties appProperties;
        std::unique_ptr<Window> mainWindow;
    };
}

juce::JUCEApplicationBase* juce_CreateApplication()
{
    return new App();
}

#endif
