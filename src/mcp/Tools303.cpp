#include "Tools303.h"

#include <algorithm>
#include <cmath>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Pattern.h"
#include "Processor.h"

namespace cs303
{
    namespace
    {
        using coolsynth::mcp::imageResult;
        using coolsynth::mcp::objectSchema;
        using coolsynth::mcp::textResult;
        using coolsynth::mcp::Tool;
        using coolsynth::mcp::typeSchema;

        juce::String db(float gain)
        {
            return gain > 0.0f ? juce::String(juce::Decibels::gainToDecibels(gain), 1) + " dBFS" : juce::String("silence");
        }

        /** The panel and the transport as the screen shows them: every value by the name set_values takes. */
        juce::var describe(Processor& p)
        {
            const auto project = toVar(p.project());
            auto* o = new juce::DynamicObject();
            o->setProperty("panel", project["panel"]);
            o->setProperty("bpm", project["bpm"]);
            o->setProperty("playing", p.isPlaying());
            o->setProperty("pattern", p.playingPattern() + 1);
            o->setProperty("variation", p.playingVariationB() ? "B" : "A");
            o->setProperty("devilFishPlaying", p.isDevilFishPlaying());
            o->setProperty("outputLevel", db(p.outputLevel()));
            return juce::var(o);
        }

        void runOn(const coolsynth::mcp::Server::Runner& runner, std::function<void()> job)
        {
            if (runner)
                runner(std::move(job));
            else
                job();
        }
    }

    std::vector<Tool> tools303(Processor& processor, coolsynth::mcp::Server::Runner runner)
    {
        std::vector<Tool> tools;

        tools.push_back({ "get_sound",
                          "The panel as it stands: every knob 0..1 (cutoff, resonance, envMod, decay, accent, tuning, "
                          "volume, drive, tone, feedback, mix), the switches (square, driveOn, delayOn, delayDivision 0..3), "
                          "the Mods panel's trimmers (tm3, tm4, tm5, c11 in uF) and the Devil Fish (devilFish on or off, "
                          "overdrive, slideTime, softAttack, normalDecay, accentDecay, tracking, filterFm 0..1; sweepSpeed "
                          "0 fast 1 normal 2 slow, accentSweep 2 stock 1 high res 0 off, muffler 0 off 1 soft 2 hard); the "
                          "tempo, whether it plays, the pattern, and the output's level now.",
                          objectSchema({}),
                          [&processor](const juce::var&) { return textResult(juce::JSON::toString(describe(processor), false)); } });

        tools.push_back({ "set_values",
                          "Sets panel values by the names get_sound gives (as the knobs and switches would), and bpm; "
                          "anything out of range is held to its range. One undo step. Answers with the panel afterwards.",
                          objectSchema({ { "values", typeSchema("object", "e.g. {\"cutoff\": 0.3, \"devilFish\": true, \"muffler\": 1, \"bpm\": 125}") } },
                                       { "values" }),
                          [&processor](const juce::var& args)
                          {
                              const auto* values = args["values"].getDynamicObject();
                              if (values == nullptr)
                                  return textResult("No values given.", true);
                              auto whole = toVar(processor.project());
                              auto* panel = whole["panel"].getDynamicObject();
                              juce::StringArray unknown;
                              for (const auto& nv : values->getProperties())
                              {
                                  if (nv.name.toString() == "bpm")
                                      whole.getDynamicObject()->setProperty("bpm", nv.value);
                                  else if (panel != nullptr && panel->hasProperty(nv.name))
                                      panel->setProperty(nv.name, nv.value);
                                  else
                                      unknown.add(nv.name.toString());
                              }
                              if (! unknown.isEmpty())
                                  return textResult("Not on the panel: " + unknown.joinIntoString(", "), true);
                              const auto changed = projectFromVar(whole);
                              processor.edit([&changed](Project& p)
                              {
                                  p.panel = changed.panel;
                                  p.bpm = changed.bpm;
                              });
                              return textResult(juce::JSON::toString(describe(processor), false));
                          } });

        tools.push_back({ "transport",
                          "Starts or stops the sequencer, and picks the pattern it plays (1 to 16, variation A or B).",
                          objectSchema({ { "play", typeSchema("boolean", "true starts, false stops") },
                                         { "pattern", typeSchema("integer", "1 to 16, optional") },
                                         { "variation", typeSchema("string", "'A' or 'B', optional") } }),
                          [&processor](const juce::var& args)
                          {
                              if (args.hasProperty("pattern"))
                                  processor.hearPattern(juce::jlimit(1, 16, static_cast<int>(args["pattern"])) - 1,
                                                        args["variation"].toString() == "B");
                              if (args.hasProperty("play"))
                                  processor.setPlaying(static_cast<bool>(args["play"]));
                              return textResult(juce::String("Asked: ") + (args.hasProperty("play") ? (static_cast<bool>(args["play"]) ? "play" : "stop") : "no change")
                                                + (args.hasProperty("pattern") ? ", pattern " + args["pattern"].toString() : juce::String()) + ".");
                          } });

        tools.push_back({ "play",
                          "Holds keys of the panel's keyboard in turn while stopped (key 0 is the lowest C, 12 the next C, "
                          "36 the top C), as a finger would, and tells the loudest output while held and after. Accent: the "
                          "Devil Fish's red button held throughout (on a Devil Fish only).",
                          objectSchema({ { "keys", typeSchema("array", "Key numbers 0..36, played one after another") },
                                         { "seconds", typeSchema("number", "How long each key is held, 0.5 by default, at most 5") },
                                         { "gap", typeSchema("number", "Silence between keys, 0.1 s by default") },
                                         { "accent", typeSchema("boolean", "Hold the Devil Fish's Accent button") },
                                         { "release", typeSchema("number", "Seconds listened to after the last key, 0.5 by default") } },
                                       { "keys" }),
                          [&processor, runner](const juce::var& args)
                          {
                              const auto* keys = args["keys"].getArray();
                              if (keys == nullptr || keys->isEmpty())
                                  return textResult("No keys given.", true);
                              const double held = juce::jlimit(0.02, 5.0, args.hasProperty("seconds") ? static_cast<double>(args["seconds"]) : 0.5);
                              const double gap = juce::jlimit(0.0, 5.0, args.hasProperty("gap") ? static_cast<double>(args["gap"]) : 0.1);
                              const double after = juce::jlimit(0.0, 10.0, args.hasProperty("release") ? static_cast<double>(args["release"]) : 0.5);
                              const bool accent = static_cast<bool>(args["accent"]);
                              runOn(runner, [&processor, accent] { processor.setPlaying(false); processor.holdAccent(accent); });
                              (void) processor.takeLoudest();
                              float loudHeld = 0.0f;
                              for (const auto& k : *keys)
                              {
                                  const int key = juce::jlimit(0, 36, static_cast<int>(k));
                                  runOn(runner, [&processor, key] { processor.audition(key); });
                                  juce::Thread::sleep(static_cast<int>(held * 1000.0));
                                  loudHeld = std::max(loudHeld, processor.takeLoudest());
                                  runOn(runner, [&processor] { processor.audition(-1); });
                                  juce::Thread::sleep(static_cast<int>(gap * 1000.0));
                              }
                              juce::Thread::sleep(static_cast<int>(after * 1000.0));
                              const float loudAfter = processor.takeLoudest();
                              runOn(runner, [&processor] { processor.holdAccent(false); });
                              return textResult("Played " + juce::String(keys->size()) + " keys, " + juce::String(held, 2) + " s each: loudest while held "
                                                + db(loudHeld) + ", after " + db(loudAfter) + ".");
                          },
                          true });

        tools.push_back({ "record",
                          "Records the app's output (its left channel, at the audio device's rate) for so many seconds into "
                          "a WAV file and gives its path, its loudest sample and its level, so it can be measured or "
                          "compared. With play, the sequencer starts with the recording and stops after it; with keys, those "
                          "keys are held in turn instead (as the play tool does).",
                          objectSchema({ { "seconds", typeSchema("number", "How long, 0.1 to 60") },
                                         { "play", typeSchema("boolean", "Start the sequencer for the recording") },
                                         { "keys", typeSchema("array", "Or: keys 0..36 held in turn, each for keySeconds") },
                                         { "keySeconds", typeSchema("number", "Each key's length with keys, 0.5 by default") },
                                         { "accent", typeSchema("boolean", "With keys: the Devil Fish's Accent button held") },
                                         { "file", typeSchema("string", "Optional path for the WAV; a temporary file by default") } },
                                       { "seconds" }),
                          [&processor, runner](const juce::var& args)
                          {
                              const double seconds = juce::jlimit(0.1, 60.0, static_cast<double>(args["seconds"]));
                              const bool play = static_cast<bool>(args["play"]);
                              const auto* keys = args["keys"].getArray();
                              const double keySeconds = juce::jlimit(0.02, 5.0, args.hasProperty("keySeconds") ? static_cast<double>(args["keySeconds"]) : 0.5);
                              const bool accent = static_cast<bool>(args["accent"]);
                              runOn(runner, [&processor, seconds, play, accent, keys]
                              {
                                  if (keys != nullptr)
                                      processor.setPlaying(false);
                                  processor.holdAccent(keys != nullptr && accent);
                                  processor.startCapture(seconds);
                                  if (play)
                                      processor.setPlaying(true);
                              });
                              const double started = juce::Time::getMillisecondCounterHiRes() * 0.001;
                              if (keys != nullptr)
                                  for (const auto& k : *keys)
                                  {
                                      const int key = juce::jlimit(0, 36, static_cast<int>(k));
                                      runOn(runner, [&processor, key] { processor.audition(key); });
                                      juce::Thread::sleep(static_cast<int>(keySeconds * 1000.0));
                                      runOn(runner, [&processor] { processor.audition(-1); });
                                      if (juce::Time::getMillisecondCounterHiRes() * 0.001 - started > seconds)
                                          break;
                                  }
                              const int wanted = static_cast<int>(processor.capture().size());
                              while (processor.captured() < wanted && juce::Time::getMillisecondCounterHiRes() * 0.001 - started < seconds + 5.0)
                                  juce::Thread::sleep(20);
                              runOn(runner, [&processor, play, keys]
                              {
                                  if (play)
                                      processor.setPlaying(false);
                                  if (keys != nullptr)
                                      processor.holdAccent(false);
                              });
                              const int n = processor.captured();
                              if (n <= 0)
                                  return textResult("Nothing came out: is the audio device running?", true);
                              auto file = args["file"].toString().isNotEmpty()
                                              ? juce::File(args["file"].toString())
                                              : juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("CoolSynth303")
                                                    .getChildFile("record-" + juce::Time::getCurrentTime().formatted("%H%M%S") + ".wav");
                              file.getParentDirectory().createDirectory();
                              file.deleteFile();
                              const auto& data = processor.capture();
                              float peak = 0.0f;
                              double sum = 0.0;
                              for (int i = 0; i < n; ++i)
                              {
                                  peak = std::max(peak, std::abs(data[static_cast<size_t>(i)]));
                                  sum += static_cast<double>(data[static_cast<size_t>(i)]) * data[static_cast<size_t>(i)];
                              }
                              {
                                  auto stream = std::unique_ptr<juce::OutputStream>(file.createOutputStream());
                                  if (stream == nullptr)
                                      return textResult("Cannot write " + file.getFullPathName(), true);
                                  juce::WavAudioFormat wav;
                                  auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions {}
                                                                                .withSampleRate(processor.captureRate())
                                                                                .withNumChannels(1)
                                                                                .withBitsPerSample(24));
                                  if (writer == nullptr)
                                      return textResult("Cannot write a WAV to " + file.getFullPathName(), true);
                                  const float* channels[] = { data.data() };
                                  writer->writeFromFloatArrays(channels, 1, n);
                              }
                              return textResult("Recorded " + juce::String(n / processor.captureRate(), 2) + " s at "
                                                + juce::String(processor.captureRate(), 0) + " Hz into " + file.getFullPathName()
                                                + ": loudest " + db(peak) + ", rms " + db(static_cast<float>(std::sqrt(sum / n))) + ".");
                          },
                          true });

        tools.push_back({ "screenshot",
                          "A picture of CoolSynth 303's window as the player sees it now.",
                          objectSchema({}),
                          [&processor](const juce::var&)
                          {
                              auto* editor = processor.getActiveEditor();
                              if (editor == nullptr || editor->getWidth() <= 0)
                                  return textResult("No window to picture.", true);
                              const auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
                              juce::MemoryOutputStream png;
                              juce::PNGImageFormat().writeImageToStream(image, png);
                              return imageResult(png.getMemoryBlock(), "CoolSynth 303's window, " + juce::String(image.getWidth()) + " x "
                                                                           + juce::String(image.getHeight()) + " pixels.");
                          } });

        return tools;
    }
}
