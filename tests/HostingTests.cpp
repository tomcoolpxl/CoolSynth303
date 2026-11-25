// The VST3 as a DAW loads it: found in its bundle, made, prepared, played from the host's transport, its state
// saved and given back. Skipped when the VST3 has not been built (the CI builds the tests only).

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

namespace
{
    class HostingTests final : public juce::UnitTest
    {
    public:
        HostingTests() : juce::UnitTest("Hosting", "CoolSynth303") {}

        void runTest() override
        {
            beginTest("the_vst3_loads_plays_with_the_host_and_keeps_its_state");
            const juce::File bundle = juce::File(COOLSYNTH303_BINARY_DIR).getChildFile("CoolSynth303_artefacts/" COOLSYNTH303_CONFIG "/VST3/CoolSynth 303.vst3"); // the test's own build
            if (! bundle.exists())
            {
                logMessage("no VST3 built at " + bundle.getFullPathName() + ": skipped");
                expect(true);
                return;
            }
            juce::VST3PluginFormat format;
            juce::OwnedArray<juce::PluginDescription> found;
            format.findAllTypesForFile(found, bundle.getFullPathName());
            expectEquals(found.size(), 1);
            if (found.isEmpty())
                return;
            expectEquals(found[0]->name, juce::String("CoolSynth 303"));
            expect(found[0]->isInstrument);

            juce::String error;
            auto plugin = format.createInstanceFromDescription(*found[0], 48000.0, 480, error);
            expect(plugin != nullptr, error);
            if (plugin == nullptr)
                return;

            struct Head final : juce::AudioPlayHead
            {
                double ppq = 0.0;
                juce::Optional<PositionInfo> getPosition() const override
                {
                    PositionInfo info;
                    info.setIsPlaying(true);
                    info.setPpqPosition(ppq);
                    info.setBpm(125.0);
                    return info;
                }
            } head;
            plugin->setPlayHead(&head);
            plugin->setPlayConfigDetails(0, 2, 48000.0, 480);
            plugin->prepareToPlay(48000.0, 480);
            juce::AudioBuffer<float> buffer(2, 480);
            double energy = 0.0;
            for (int b = 0; b < 200; ++b)
            {
                juce::MidiBuffer midi;
                buffer.clear();
                plugin->processBlock(buffer, midi);
                head.ppq += 480.0 / 48000.0 * 125.0 / 60.0;
                for (int i = 0; i < 480; ++i)
                    energy += static_cast<double>(buffer.getSample(0, i)) * buffer.getSample(0, i);
            }
            const double rms = std::sqrt(energy / (200.0 * 480.0));
            logMessage("the VST3 under a playing host: rms " + juce::String(juce::Decibels::gainToDecibels(rms), 1) + " dBFS");
            expect(rms > 0.01, "it plays with the host's transport");

            juce::MemoryBlock state;
            plugin->getStateInformation(state);
            // the host keeps the VST3's state as base64 in its own XML: the project's JSON (tens of kilobytes) is in it
            expect(state.getSize() > 10000, "its state holds the project: " + juce::String(static_cast<int>(state.getSize())) + " bytes");
            auto second = format.createInstanceFromDescription(*found[0], 48000.0, 480, error);
            expect(second != nullptr);
            if (second != nullptr)
            {
                second->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
                juce::MemoryBlock back;
                second->getStateInformation(back);
                expect(back == state, "a second instance takes the state as it was");
                second->releaseResources();
            }
            plugin->releaseResources();
            plugin->setPlayHead(nullptr);
        }
    };

    static HostingTests hostingTests;
}
