// The MCP server's tools (Claude's way into the standalone app): listed, read, set, and the capture the record tool
// writes from, without a socket or an audio device (the server's own HTTP handling is CoolSynth's, tested there).

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "Processor.h"
#include "mcp/Tools303.h"

namespace
{
    class McpTests final : public juce::UnitTest
    {
    public:
        McpTests() : juce::UnitTest("Mcp", "CoolSynth303") {}

        void runTest() override
        {
            auto processor = std::make_unique<cs303::Processor>();
            processor->prepareToPlay(48000.0, 480);
            const coolsynth::mcp::Server::Runner direct = [](std::function<void()> job) { job(); };
            coolsynth::mcp::Server server("CoolSynth 303", "test", cs303::tools303(*processor, direct), direct);
            int id = 0;
            const auto call = [&](const juce::String& method, const juce::var& params)
            {
                auto* o = new juce::DynamicObject();
                o->setProperty("jsonrpc", "2.0");
                o->setProperty("id", ++id);
                o->setProperty("method", method);
                o->setProperty("params", params);
                return server.handleMessage(juce::var(o));
            };
            const auto tool = [&](const juce::String& toolName, const juce::String& argumentsJson)
            {
                auto* p = new juce::DynamicObject();
                p->setProperty("name", toolName);
                p->setProperty("arguments", juce::JSON::parse(argumentsJson));
                const auto answer = call("tools/call", juce::var(p));
                return answer["result"]["content"][0]["text"].toString();
            };

            beginTest("the_tools_are_listed");
            {
                const auto list = call("tools/list", juce::var(new juce::DynamicObject()));
                juce::StringArray names;
                if (const auto* tools = list["result"]["tools"].getArray())
                    for (const auto& t : *tools)
                        names.add(t["name"].toString());
                for (const char* wanted : { "get_sound", "set_values", "transport", "play", "record", "screenshot", "view" })
                    expect(names.contains(wanted), juce::String("listed: ") + wanted);
            }

            beginTest("view_lifts_and_lowers_the_lid");
            {
                expectEquals(tool("view", "{\"lid\": true}"), juce::String("No window open."));
                std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditorAndMakeActive());
                editor->setSize(1280, 800);
                expectEquals(tool("view", "{\"lid\": true}"), juce::String("The lid is lifted."));
                expectEquals(tool("view", "{\"lid\": false}"), juce::String("The lid is down."));
                processor->editorBeingDeleted(editor.get());
                editor = nullptr;
            }

            beginTest("get_sound_reads_the_panel_and_set_values_changes_it");
            {
                const auto before = juce::JSON::parse(tool("get_sound", "{}"));
                expect(before["panel"].hasProperty("cutoff") && before["panel"].hasProperty("devilFish"), "the panel's values by name");
                const auto after = juce::JSON::parse(tool("set_values", "{\"values\": {\"cutoff\": 0.25, \"devilFish\": true, \"muffler\": 2, \"bpm\": 125}}"));
                expectWithinAbsoluteError(static_cast<double>(processor->project().panel.cutoff), 0.25, 1.0e-6);
                expect(processor->project().panel.devilFish);
                expectEquals(processor->project().panel.muffler, 2);
                expectWithinAbsoluteError(processor->project().bpm, 125.0, 1.0e-9);
                expectWithinAbsoluteError(static_cast<double>(after["panel"]["cutoff"]), 0.25, 1.0e-6);
                expect(tool("set_values", "{\"values\": {\"nothing\": 1}}").startsWith("Not on the panel"), "an unknown name is refused");
                expect(processor->canUndo(), "one undo step");
            }

            beginTest("a_capture_keeps_the_output_while_armed");
            {
                processor->startCapture(0.2);
                processor->audition(12);
                juce::AudioBuffer<float> buffer(2, 480);
                juce::MidiBuffer midi;
                for (int b = 0; b < 30; ++b)
                {
                    buffer.clear();
                    processor->processBlock(buffer, midi);
                }
                processor->audition(-1);
                expectEquals(processor->captured(), 9600, "0.2 s at 48 kHz, then no more");
                float peak = 0.0f;
                for (const float v : processor->capture())
                    peak = std::max(peak, std::abs(v));
                expectGreaterThan(peak, 0.01f, "the held key is in it");
                expectGreaterThan(processor->takeLoudest(), 0.01f, "the loudest sample is kept for the play tool");
            }
        }
    };

    static McpTests mcpTests;
}
