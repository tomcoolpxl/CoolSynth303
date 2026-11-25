#pragma once

#include <memory>

#include <juce_audio_devices/juce_audio_devices.h>

/*
    The standalone app's audio and MIDI devices, for the options window (D349, as CoolSynth 808's). In a plugin, and
    in the tests, there are none: the host owns its devices, and the options window leaves those pages out.
*/
namespace cs303
{
    /** The standalone app's device manager, or null when this is not the standalone app. */
    juce::AudioDeviceManager* standaloneDeviceManager();
    /** Keeps the devices as now chosen, at once, in JUCE's own settings file: JUCE keeps them only when the app closes. */
    void saveStandaloneDeviceState();
    /** MIDI clock out on time, as CoolSynth 808's: JUCE's standalone player sends a block's MIDI all at once from the
        audio thread unless the output runs its own sending thread, and then each message leaves at its own sample.
        This keeps that thread running on whichever output is chosen. Null in a plugin and in the tests. */
    [[nodiscard]] std::unique_ptr<juce::Timer> makeStandaloneMidiOutKeeper();
}
