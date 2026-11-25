#include "StandaloneDevices.h"

#if JucePlugin_Build_Standalone
 #include <juce_audio_processors/juce_audio_processors.h>
 #include <juce_audio_utils/juce_audio_utils.h>
 #include <juce_gui_extra/juce_gui_extra.h>
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

namespace cs303
{
#if JucePlugin_Build_Standalone
    juce::AudioDeviceManager* standaloneDeviceManager()
    {
        auto* holder = juce::StandalonePluginHolder::getInstance();
        return holder != nullptr ? &holder->deviceManager : nullptr;
    }

    void saveStandaloneDeviceState()
    {
        if (auto* holder = juce::StandalonePluginHolder::getInstance())
        {
            holder->saveAudioDeviceState();
            if (auto* file = dynamic_cast<juce::PropertiesFile*>(holder->settings.get()))
                file->saveIfNeeded();
        }
    }

    namespace
    {
        class MidiOutKeeper final : public juce::Timer
        {
        public:
            MidiOutKeeper() { startTimer(500); }

            void timerCallback() override
            {
                if (auto* holder = juce::StandalonePluginHolder::getInstance())
                    if (auto* out = holder->deviceManager.getDefaultMidiOutput())
                        if (! out->isBackgroundThreadRunning())
                            out->startBackgroundThread();
            }
        };
    }

    std::unique_ptr<juce::Timer> makeStandaloneMidiOutKeeper() { return std::make_unique<MidiOutKeeper>(); }
#else
    juce::AudioDeviceManager* standaloneDeviceManager() { return nullptr; }
    void saveStandaloneDeviceState() {}
    std::unique_ptr<juce::Timer> makeStandaloneMidiOutKeeper() { return {}; }
#endif
}
