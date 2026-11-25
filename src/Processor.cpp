#include "Processor.h"

#include <cmath>

#include "Editor.h"
#include "StandaloneDevices.h"
#include "mcp/Tools303.h"
#include "synth/GlobalDelay.h"
#include "synth/HalfBand2x.h"
#include "synth/v3/ShaperProcessors.h"
#include "synth/v3/Tb303Lid.h"
#include "synth/v3/Tb303Voice.h"

namespace cs303
{
    using coolsynth::synth::v3::Tb303Voice;
    using coolsynth::synth::v3::DriveProcessor;

    namespace
    {
        constexpr int maxUndo = 64;
        constexpr int midiNoteAsWritten = 60; // a key or MIDI note at middle C plays the pattern as written

        /** A crash in the standalone leaves its call stack in %TEMP%\CoolSynth303\crash.txt, named by the symbols. */
        void writeCrashStack(void*)
        {
            const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("CoolSynth303").getChildFile("crash.txt");
            file.getParentDirectory().createDirectory();
            file.appendText(juce::Time::getCurrentTime().toString(true, true, true, true) + "\n" + juce::SystemStats::getStackBacktrace() + "\n");
        }

        /** VOLUME's slew: the voice's pot moves at most this much of its turn every volumeEvery voice samples (a whole
            turn in some 0.17 s at 96 kHz), so a knob turned fast steps by under 0.1 dB, and the circuit is not
            re-analysed every sample. */
        constexpr double volumeSlew = 0.002;
        constexpr int volumeEvery = 32;

        /** The output stage's soft saturation: straight up to the knee (-3 dBFS), then a tanh from there toward full
            scale, its slope 1 at the knee so nothing below it changes and nothing above it jumps. */
        float outputStage(float x) noexcept
        {
            constexpr float knee = 0.7079458f, room = 1.0f - knee;
            const float a = std::abs(x);
            if (a <= knee)
                return x;
            return std::copysign(knee + room * std::tanh((a - knee) / room), x);
        }
    }

    Processor::Processor(bool offlineRender)
        : juce::AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          offline(offlineRender),
          model(std::make_unique<Project>(defaultProject())),
          pending(std::make_unique<AudioCopy>()),
          audio(std::make_unique<AudioCopy>()),
          voice(std::make_unique<Tb303Voice>()),
          fishVoice(std::make_unique<Tb303Voice>(true)),
          active(voice.get()),
          decimator(std::make_unique<coolsynth::synth::HalfBandDecimator2x>()),
          drive(std::make_unique<DriveProcessor>()),
          delay(std::make_unique<coolsynth::synth::GlobalDelay>())
    {
        for (auto& k : liveKnobs)
            k.store(0.0f);
        lidRing.assign(static_cast<size_t>(lidFrames) * static_cast<size_t>(lidStride()), 0.0f);
        push();
        // The accuracy (D349): the app's, from its settings, in the standalone and a plugin; Best in the tests.
        SyncChoice sync;
        bool mcpOn = false;
        if (wrapperType != wrapperType_Undefined)
        {
            juce::PropertiesFile file(settingsOptions());
            accuracyLevel = juce::jlimit(0, 1, file.getIntValue("accuracy", 0));
            sync = syncChoiceFrom(file); // D361
            mcpOn = mcpChoiceFrom(file); // CoolSynth's D363
        }
        // Claude's way in, in the standalone app only (a plugin in a DAW opens no port, nor does an export), and
        // only once switched on in Options (CoolSynth's D363).
        if (wrapperType == wrapperType_Standalone && ! offline)
        {
            juce::SystemStats::setApplicationCrashHandler(writeCrashStack);
            mcpSwitchable = true;
            mcpListenPort = mcpPort;
            mcpChosen = mcpOn;
            if (mcpChosen)
                startMcpServer();
            midiOutKeeper = makeStandaloneMidiOutKeeper(); // MIDI clock out at its own samples, as the 808's
            // The beat sync, the standalone's only (D361): in a DAW its transport rules, an export renders alone.
            startBeatSync(sync, true, {});
        }
    }

    int Processor::lidSeries() noexcept { return coolsynth::synth::v3::Tb303Lid::count(); }

    juce::PropertiesFile::Options Processor::settingsOptions()
    {
        // Beside JUCE's own file for the standalone's devices, as CoolSynth 808's (D339).
        juce::PropertiesFile::Options options;
        options.applicationName = "CoolSynth303";
        options.filenameSuffix = ".settings";
        options.folderName = "CoolSynth303";
        options.osxLibrarySubFolder = "Application Support";
        return options;
    }

    void Processor::setAccuracy(int level)
    {
        level = juce::jlimit(0, 1, level);
        if (level == accuracyLevel)
            return;
        accuracyLevel = level;
        if (wrapperType != wrapperType_Undefined)
        {
            juce::PropertiesFile file(settingsOptions());
            file.setValue("accuracy", level);
            file.saveIfNeeded();
        }
        if (! prepared)
            return;
        // The voice's tolerances and perhaps its rate change: prepared again while the audio thread skips its blocks.
        suspendProcessing(true);
        prepareToPlay(hostRate, preparedBlock);
        suspendProcessing(false);
    }

    Processor::~Processor()
    {
        alive->store(false); // a tool's job still queued finds the processor gone and does nothing
        mcp = nullptr;
        syncTicker.reset();
        beatSync.reset(); // says goodbye to the other peers; the audio has stopped
    }

    // ---------------------------------------------------------------- the beat sync (CoolSynth's D361)
    SyncChoice syncChoiceFrom(const juce::PropertySet& settings)
    {
        SyncChoice c;
        c.on = settings.getBoolValue("beatSync", c.on);
        c.sharing = settings.getBoolValue("beatSyncSharing", c.sharing);
        return c;
    }

    void keepSyncChoice(juce::PropertySet& settings, const SyncChoice& choice)
    {
        settings.setValue("beatSync", choice.on);
        settings.setValue("beatSyncSharing", choice.sharing);
    }

    juce::String syncKeyText(int peers) { return peers > 0 ? "Sync " + juce::String(peers) : juce::String("Sync"); }

    namespace
    {
        class Ticker final : public juce::Timer
        {
        public:
            explicit Ticker(std::function<void()> f) : tick(std::move(f)) {}
            void timerCallback() override { tick(); }

        private:
            std::function<void()> tick;
        };
    }

    void Processor::startBeatSync(const SyncChoice& choice, bool network, coolsynth::sync::BeatSync::Options options)
    {
        if (beatSync != nullptr || offline)
            return;
        beatSync = std::make_unique<coolsynth::sync::BeatSync>(model->bpm, std::move(options));
        beatSync->setStartStopSharing(choice.sharing);
        syncNetwork = network;
        if (network)
            beatSync->setEnabled(choice.on); // switched on, it founds a session of its own and meets the others
        syncOn.store(choice.on, std::memory_order_release);
        if (network)
        {
            syncTicker = std::make_unique<Ticker>([this] { syncTick(); });
            syncTicker->startTimerHz(10);
        }
    }

    void Processor::useBeatSyncForTesting(bool network, const juce::File& settings)
    {
        coolsynth::sync::BeatSync::Options options;
        options.loopbackOnly = true; // this computer only
        syncSettingsFile = settings;
        startBeatSync({}, network, std::move(options));
        if (syncTicker != nullptr)
            syncTicker->stopTimer(); // the test calls syncTick itself
    }

    void Processor::keepSyncChoiceNow()
    {
        // The app's settings, as the accuracy (the tests' own file instead; none for a test without one).
        if (syncSettingsFile == juce::File() && (wrapperType == wrapperType_Undefined || offline))
            return;
        auto file = syncSettingsFile == juce::File() ? std::make_unique<juce::PropertiesFile>(settingsOptions())
                                                     : std::make_unique<juce::PropertiesFile>(syncSettingsFile, settingsOptions());
        keepSyncChoice(*file, { beatSyncOn(), startStopSharing() });
        file->saveIfNeeded();
    }

    void Processor::setBeatSyncOn(bool on)
    {
        if (beatSync == nullptr || on == beatSyncOn())
            return;
        if (syncNetwork)
            beatSync->setEnabled(on);
        syncOn.store(on, std::memory_order_release);
        keepSyncChoiceNow();
    }

    void Processor::setStartStopSharing(bool on)
    {
        if (beatSync == nullptr || on == startStopSharing())
            return;
        beatSync->setStartStopSharing(on);
        keepSyncChoiceNow();
    }

    void Processor::syncTick()
    {
        if (beatSync == nullptr)
            return;
        // The output's latency (the beat sync's design, "Following the transport sample-accurately"): the device's
        // own and its buffer, read as it runs, so the beat is placed where it is heard.
        if (auto* devices = standaloneDeviceManager())
            if (auto* device = devices->getCurrentAudioDevice(); device != nullptr && device->isPlaying())
                if (const double r = device->getCurrentSampleRate(); r > 0.0)
                    outputLatencyMicros.store(std::llround(1.0e6 * (device->getOutputLatencyInSamples() + device->getCurrentBufferSizeSamples()) / r),
                                              std::memory_order_relaxed);
        // A tempo another peer set becomes the panel's, so it stays when they leave; only once the audio thread has
        // the panel's own last tempo, or an edit on its way would be undone. No undo step: the session's, not yours.
        if (! beatSyncOn() || syncPeersNow.load(std::memory_order_acquire) <= 0)
            return;
        const double heard = syncBpmNow.load(std::memory_order_acquire);
        const double tempo = model->bpm;
        if (heard > 0.0 && syncPanelTempoNow.load(std::memory_order_acquire) == tempo
            && coolsynth::sync::tempoFromBpm(heard) != coolsynth::sync::tempoFromBpm(tempo))
            edit([heard](Project& p) { p.bpm = heard; }, false);
    }

    juce::Optional<juce::AudioPlayHead::PositionInfo> Processor::SyncPlayHead::getPosition() const
    {
        PositionInfo info;
        // The tempo the block plays at: the sync's beats a sample, so one block ends where the next begins.
        const double played = beats.beatsPerSample * 60.0 * sampleRate;
        info.setBpm(played > 0.0 ? played : beats.bpm);
        info.setPpqPosition(beats.beat);
        info.setIsPlaying(beats.playing && beats.beat + beats.beatsPerSample * samples > 0.0);
        info.setTimeSignature(juce::AudioPlayHead::TimeSignature { 4, 4 });
        const double bar = std::floor(beats.beat / 4.0);
        info.setPpqPositionOfLastBarStart(bar * 4.0);
        info.setBarCount(static_cast<std::int64_t>(bar));
        return info;
    }

    void Processor::startCapture(double seconds)
    {
        captureArmed.store(false, std::memory_order_release);
        // The audio thread may still be inside its last copy: wait out a block before the buffer is touched.
        juce::Thread::sleep(static_cast<int>(1000.0 * maxBlock / std::max(1.0, hostRate)) + 5);
        captureBuffer.assign(static_cast<size_t>(juce::jlimit(0.1, 60.0, seconds) * hostRate), 0.0f);
        captureCount.store(0, std::memory_order_release);
        captureArmed.store(true, std::memory_order_release);
    }

    bool mcpChoiceFrom(const juce::PropertySet& settings) { return settings.getBoolValue("mcpServer", false); }

    bool mcpForcedOff() { return juce::SystemStats::getEnvironmentVariable("COOLSYNTH303_NO_MCP", {}).isNotEmpty(); }

    void Processor::setMcpServerOn(bool on)
    {
        if (! mcpSwitchable)
            return;
        mcpChosen = on;
        // kept at once in the app's settings (the tests' own file instead)
        auto file = mcpSettingsFile == juce::File() ? std::make_unique<juce::PropertiesFile>(settingsOptions())
                                                    : std::make_unique<juce::PropertiesFile>(mcpSettingsFile, settingsOptions());
        file->setValue("mcpServer", on);
        file->saveIfNeeded();
        if (on)
            startMcpServer();
        else
            stopMcpServer();
    }

    void Processor::useMcpSwitchForTesting(int port, const juce::File& settings)
    {
        stopMcpServer();
        mcpSwitchable = true;
        mcpListenPort = port;
        mcpSettingsFile = settings;
        mcpChosen = settings.existsAsFile() && mcpChoiceFrom(juce::PropertiesFile(settings, settingsOptions()));
        if (mcpChosen)
            startMcpServer();
    }

    void Processor::stopMcpServer()
    {
        mcp = nullptr; // its thread stopped, its port let go
        mcpStatusText = "Off";
    }

    void Processor::startMcpServer()
    {
        if (mcp != nullptr)
            return;
        if (mcpForcedOff())
        {
            mcpStatusText = "Off: COOLSYNTH303_NO_MCP";
            return;
        }
        // A tool runs on the message thread, as the panel's own edits do, and the server waits for it (10 s at most).
        coolsynth::mcp::Server::Runner runner = [alive = alive](std::function<void()> job)
        {
            if (juce::MessageManager::getInstance()->isThisTheMessageThread())
            {
                job();
                return;
            }
            auto done = std::make_shared<juce::WaitableEvent>();
            juce::MessageManager::callAsync([alive, done, job = std::move(job)]
            {
                if (alive->load())
                    job();
                done->signal();
            });
            done->wait(10000);
        };
        mcp = std::make_unique<coolsynth::mcp::Server>("CoolSynth 303", "0.1", tools303(*this, runner), runner);
        if (mcp->start(mcpListenPort))
            mcpStatusText = "On: 127.0.0.1:" + juce::String(mcpListenPort);
        else
        {
            mcp = nullptr;
            mcpStatusText = "Port " + juce::String(mcpListenPort) + " is taken";
        }
    }

    bool Processor::isBusesLayoutSupported(const BusesLayout& layouts) const
    {
        return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
               || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
    }

    void Processor::prepareToPlay(double sampleRate, int samplesPerBlock)
    {
        hostRate = sampleRate > 0.0 ? sampleRate : 48000.0;
        // The accuracy (D349): a host's offline render at Best whatever the setting.
        accuracyNow = isNonRealtime() ? 0 : accuracyLevel;
        // the voice near 96 kHz (D294): twice 44.1 or 48 kHz, a fast device's own rate; Light the host's own
        factor = hostRate < 80000.0 && accuracyNow == 0 ? 2 : 1;
        voiceRate = factor * hostRate;
        preparedBlock = samplesPerBlock;
        lidEvery = std::max(1, juce::roundToInt(voiceRate / lidRate));
        lidPhase = 0;
        const int block = std::max(samplesPerBlock, maxBlock);
        voiceBuffer.assign(static_cast<size_t>(2 * block), 0.0f);
        driveScratch.assign(static_cast<size_t>(2 * block), 0.0f);
        stereo.setSize(2, block, false, true, true);
        for (auto* v : { voice.get(), fishVoice.get() })
        {
            v->prepare(voiceRate);
            v->setLighter(accuracyNow >= 1);
            v->settle(noteVolts(12));
        }
        decimator->reset();
        coolsynth::synth::v3::SlotMemory memory;
        drive->prepare(voiceRate, block, memory);
        delay->prepare(hostRate, block, 2);
        clock.prepare(hostRate);
        clock.setBlockLate(wrapperType == wrapperType_Standalone); // the standalone's MIDI comes a block late (D359)
        blockClock.reset(hostRate); // the device started, or its rate changed (D361)
        samplesSoFar = 0;
        playingByClock = endedUnderClock = false;
        sequencer.reset();
        playing = false;
        sixteenth = -1;
        dirty.store(true);
        prepared = true;
    }

    // ---------------------------------------------------------------- the project
    void Processor::keepUndo()
    {
        undoStack.push_back(std::make_unique<Project>(*model));
        if (static_cast<int>(undoStack.size()) > maxUndo)
            undoStack.erase(undoStack.begin());
        redoStack.clear();
    }

    void Processor::edit(const std::function<void(Project&)>& change, bool undoable)
    {
        if (undoable && (! inGesture || ! gestureKept))
        {
            keepUndo();
            gestureKept = inGesture;
        }
        change(*model);
        ++edits;
        push();
    }

    void Processor::replaceProject(const Project& p)
    {
        *model = p;
        undoStack.clear();
        redoStack.clear();
        ++edits;
        push();
    }

    void Processor::undo()
    {
        if (undoStack.empty())
            return;
        redoStack.push_back(std::make_unique<Project>(*model));
        *model = *undoStack.back();
        undoStack.pop_back();
        ++edits;
        push();
    }

    void Processor::redo()
    {
        if (redoStack.empty())
            return;
        undoStack.push_back(std::make_unique<Project>(*model));
        *model = *redoStack.back();
        redoStack.pop_back();
        ++edits;
        push();
    }

    void Processor::push()
    {
        const juce::SpinLock::ScopedLockType lock(handover);
        pending->bank = PlayBank(*model);
        pending->panel = model->panel;
        pending->bpm = model->bpm;
        dirty.store(true);
    }

    void Processor::knobMoved(Knob k, float value)
    {
        const auto i = static_cast<size_t>(k);
        edit([&](Project& p) {
            auto& pn = p.panel;
            float* knob[] = { &pn.cutoff, &pn.resonance, &pn.envMod, &pn.decay, &pn.accent };
            *knob[i] = value;
            if (recording.load() && playingNow.load())
            {
                const int step = stepNow.load();
                if (step >= 0 && step < Pattern::steps)
                    p.pattern(patternNow.load(), variationBNow.load()).motion[i][static_cast<size_t>(step)] = value;
            }
        });
    }

    // ---------------------------------------------------------------- the state
    void Processor::getStateInformation(juce::MemoryBlock& destData)
    {
        const auto json = toJson(*model);
        destData.replaceAll(json.toRawUTF8(), json.getNumBytesAsUTF8());
    }

    void Processor::setStateInformation(const void* data, int sizeInBytes)
    {
        const auto json = juce::String::fromUTF8(static_cast<const char*>(data), sizeInBytes);
        replaceProject(projectFromJson(json));
    }

    // ---------------------------------------------------------------- the audio thread
    void Processor::pullFromUi() noexcept
    {
        if (! dirty.load())
            return;
        const juce::SpinLock::ScopedTryLockType lock(handover);
        if (! lock.isLocked())
            return;
        std::swap(pending, audio);
        *pending = *audio; // the next push writes over a copy; nothing is allocated here
        dirty.store(false);
        sequencer.setBank(audio->bank);
        const auto& pn = audio->panel;
        Tb303Voice::Trimmers t;
        t.tm3 = pn.tm3;
        t.tm4 = pn.tm4;
        t.tm5 = pn.tm5;
        t.c11 = pn.c11 * 1.0e-6;
        for (auto* v : { voice.get(), fishVoice.get() })
        {
            const auto& was = v->trimmers();
            if (t.tm3 != was.tm3 || t.tm4 != was.tm4 || t.tm5 != was.tm5 || t.c11 != was.c11)
                v->setTrimmers(t);
        }
        Tb303Voice::DevilFish f;
        f.overdrive = pn.overdrive;
        f.slideTime = pn.slideTime;
        f.softAttack = pn.softAttack;
        f.normalDecay = pn.normalDecay;
        f.accentDecay = pn.accentDecay;
        f.tracking = pn.tracking;
        f.filterFm = pn.filterFm;
        f.sweepSpeed = pn.sweepSpeed;
        f.accentSweep = pn.accentSweep;
        f.muffler = pn.muffler;
        const auto& had = fishVoice->devilFish();
        if (f.overdrive != had.overdrive || f.slideTime != had.slideTime || f.softAttack != had.softAttack
            || f.normalDecay != had.normalDecay || f.accentDecay != had.accentDecay || f.tracking != had.tracking
            || f.filterFm != had.filterFm || f.sweepSpeed != had.sweepSpeed || f.accentSweep != had.accentSweep
            || f.muffler != had.muffler)
            fishVoice->setDevilFish(f);
        applyPanel(audio->panel, playing ? &sequencer.current() : nullptr); // a knob turned takes effect at once
        chooseVoice(pn.devilFish);
        restSettled = false; // a change while resting: the circuit settles again before it wakes (D340)
    }

    void Processor::chooseVoice(bool devilFish) noexcept
    {
        auto* wanted = devilFish ? fishVoice.get() : voice.get();
        if (wanted == active)
            return;
        // The other circuit takes over at rest at the pitch that plays (as a wake from rest does).
        wanted->settle(playing ? sequencer.current().volts : auditionVolts);
        active = wanted;
        fishNow.store(devilFish);
    }

    void Processor::applyPanel(const Panel& p, const StepOut* step) noexcept
    {
        const float panelKnobs[] = { p.cutoff, p.resonance, p.envMod, p.decay, p.accent };
        Tb303Voice::Knobs k;
        k.tuning = p.tuning;
        double* target[] = { &k.cutoff, &k.resonance, &k.envMod, &k.decay, &k.accent };
        for (size_t i = 0; i < static_cast<size_t>(knobCount); ++i)
        {
            const float moved = step != nullptr ? step->knob[i] : none;
            const float v = moved >= 0.0f ? moved : panelKnobs[i];
            *target[i] = v;
            liveKnobs[i].store(v);
        }
        k.square = p.square;
        if (volumeNow < 0.0)
            volumeNow = p.volume; // the first panel: VOLUME where it stands, no slew
        k.volume = volumeNow;
        voice->setKnobs(k);
        fishVoice->setKnobs(k);
    }

    void Processor::setVolumeNow(double turn) noexcept
    {
        volumeNow = turn;
        voice->setVolume(turn);
        fishVoice->setVolume(turn);
    }

    void Processor::renderSpan(float* out, int start, int count) noexcept
    {
        const auto& panel = audio->panel;
        const double bpm = juce::jlimit(20.0, 400.0, blockBpm);
        const double beatPerVoiceSample = bpm / 60.0 / voiceRate;
        const double secondsPerStep = 15.0 / bpm;
        float* v = voiceBuffer.data();
        const int n = factor * count;
        spanStart = samplesSoFar + start;
        const int key = auditionKey.load();
        if (! playing && key != lastAudition)
        {
            auditionSlide = key >= 0 && lastAudition >= 0;
            if (key >= 0)
                auditionVolts = noteVolts(key + 0);
            lastAudition = key;
            soundingNow.store(key);
        }
        // At rest: stopped, no key held and silent for a while, the circuit is not solved (it would cost most of a
        // core for nothing). It settles whole while it rests, where that costs nothing (on resting, and again after
        // any change), and the next note only wakes it: the oscillator settles at its pitch (D340). Settled on the
        // note instead, the settle's millisecond fell into the note's first block, on top of the attack's own work,
        // and took it over its time at the start of playback.
        const bool wanted = playing || key >= 0;
        if (! wanted && quietVoiceSamples > static_cast<std::int64_t>(restAfterSeconds * voiceRate))
        {
            if (! resting || ! restSettled)
            {
                active->settle(auditionVolts);
                restSettled = true;
            }
            resting = true;
            std::fill(v, v + n, 0.0f);
            if (volumeNow != static_cast<double>(panel.volume))
                setVolumeNow(panel.volume); // silent at rest: no slew needed, and the pot moves no DC
        }
        else
        {
            if (resting)
            {
                if (restSettled)
                    active->wake(playing ? sequencer.current().volts : auditionVolts);
                else
                    active->settle(playing ? sequencer.current().volts : auditionVolts);
                resting = false;
                quietVoiceSamples = 0;
            }
            renderVoice(v, n, key, beatPerVoiceSample, secondsPerStep, panel);
        }
        finishSpan(out, start, count, panel);
    }

    void Processor::renderVoice(float* v, int n, int key, double beatPerVoiceSample, double secondsPerStep, const Panel& panel) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            bool gate = false, accent = false, slide = false;
            double volts = auditionVolts;
            if (playing)
            {
                const auto s = static_cast<std::int64_t>(std::floor(beat * 4.0));
                // a clock's bend can put the beat a hair back across a step's line: the step stays; forward, every
                // step in between is taken in order (a far jump starts afresh)
                if (s > sixteenth)
                {
                    if (s - sixteenth > 64 || sixteenth < 0)
                        sixteenth = s - 1;
                    while (sixteenth < s)
                        sequencer.stepTo(++sixteenth);
                    if (stepLog != nullptr)
                        stepLog->push_back(static_cast<double>(spanStart) + static_cast<double>(i) / factor); // the tests' only
                    const auto& cur = sequencer.current();
                    applyPanel(panel, &cur);
                    stepNow.store(sequencer.playingStep());
                    patternNow.store(sequencer.playingPattern());
                    variationBNow.store(sequencer.playingVariationB());
                    chainNow.store(sequencer.chainPosition());
                    queuedNow.store(sequencer.queuedPattern());
                    soundingNow.store(cur.sounds ? juce::roundToInt((cur.volts - 1.0) * 12.0) : -1);
                }
                const auto& cur = sequencer.current();
                const double fraction = juce::jlimit(0.0, 1.0, beat * 4.0 - static_cast<double>(sixteenth));
                gate = sequencer.gateAt(fraction, secondsPerStep);
                accent = cur.accent && cur.sounds;
                slide = cur.slide;
                volts = cur.sounds ? cur.volts : auditionVolts;
                if (cur.sounds)
                    auditionVolts = cur.volts; // a rest keeps the pitch the note before left
                beat += beatPerVoiceSample;
            }
            else if (key >= 0)
            {
                gate = true;
                slide = auditionSlide;
            }
            if (accentHeld.load(std::memory_order_relaxed) && active == fishVoice.get())
                accent = gate;
            // VOLUME: the voice's own pot ahead of its mixer (D362), slewed toward the panel's
            if (i % volumeEvery == 0 && volumeNow != static_cast<double>(panel.volume))
                setVolumeNow(volumeNow + juce::jlimit(-volumeSlew, volumeSlew, static_cast<double>(panel.volume) - volumeNow));
            double y = active->process(volts, gate, accent, slide);
            if (! std::isfinite(y))
            {
                active->prepare(voiceRate);
                active->settle(volts);
                y = 0.0;
            }
            // Under the Lid: the circuit's every drawn current and voltage, lidRate times a second, while it is open
            if (lidOpen.load(std::memory_order_relaxed) && ++lidPhase >= lidEvery)
            {
                lidPhase = 0;
                const auto at = lidCount.load(std::memory_order_relaxed);
                float* f = lidRing.data() + static_cast<size_t>(at % lidFrames) * static_cast<size_t>(lidStride());
                coolsynth::synth::v3::Tb303Lid::read(*active, f);
                const int s = lidSeries();
                f[s] = gate ? 1.0f : 0.0f;
                f[s + 1] = accent ? 1.0f : 0.0f;
                f[s + 2] = slide ? 1.0f : 0.0f;
                f[s + 3] = static_cast<float>(volts);
                lidCount.store(at + 1, std::memory_order_release);
            }
            // the OUTPUT jack (its DC off through C38, C59 and C56 in the voice's output stage, D362)
            v[i] = static_cast<float>(y * outputScale);
            // silent: below -80 dBFS (at rest the VCA lets the oscillator through at about -98 dBFS, as a 303's does)
            quietVoiceSamples = gate || std::abs(v[i]) > 1.0e-4f ? 0 : quietVoiceSamples + 1;
        }
    }

    void Processor::finishSpan(float* out, int start, int count, const Panel& panel) noexcept
    {
        float* v = voiceBuffer.data();
        const int n = factor * count;
        if (panel.driveOn)
        {
            coolsynth::synth::v3::SlotParams sp;
            sp.p[DriveProcessor::drive] = 36.0f * panel.drive;
            sp.p[DriveProcessor::curve] = 0.0f;
            sp.p[DriveProcessor::tone] = panel.tone;
            sp.p[DriveProcessor::mix] = 1.0f;
            std::copy(v, v + n, driveScratch.data());
            drive->process(v, driveScratch.data(), n, sp);
        }
        for (int i = 0; i < count; ++i)
            out[start + i] = factor == 2 ? decimator->process(v[2 * i], v[2 * i + 1]) : v[i];
    }

    void Processor::sendClock(juce::MidiBuffer& midi, int numSamples) noexcept
    {
        if (clockStopPending)
        {
            midi.addEvent(juce::MidiMessage::midiStop(), 0);
            clockStopPending = false;
        }
        if (clockStartAt >= 0)
        {
            midi.addEvent(juce::MidiMessage::midiStart(), clockStartAt); // at the sample the pattern starts
            clockStartAt = -1;
        }
        // Following a clock or a host, it does not send one of its own; led by the beat sync it does (D361), so what
        // follows it by MIDI plays with the session.
        if (! playing || playingByClock || (playingByHost && ! syncInUse))
            return;
        const double perSample = juce::jlimit(20.0, 400.0, blockBpm) / 60.0 / hostRate;
        const double startBeat = beat - perSample * numSamples; // the block's start: beat has moved on in renderSpan
        for (int i = 0; i < numSamples; ++i)
        {
            const double b = startBeat + perSample * i;
            if (b >= clockBeatSent - 1.0e-9) // a hair of rounding: a start's first pulse at its own sample
            {
                midi.addEvent(juce::MidiMessage::midiClock(), i);
                clockBeatSent += 1.0 / 24.0;
            }
        }
    }

    void Processor::renderRange(int from, int to) noexcept
    {
        for (int done = from; done < to;)
        {
            const int span = std::min(maxBlock, to - done);
            renderSpan(stereo.getWritePointer(0), done, span);
            done += span;
        }
    }

    void Processor::startOrStop(bool wantPlay, bool byClock, bool byHost, double clockBeat, int at) noexcept
    {
        playing = wantPlay;
        if (playing)
        {
            playingByClock = byClock;
            playingByHost = byHost;
            sequencer.reset();
            beat = byClock || byHost ? std::max(0.0, clockBeat) : 0.0;
            sixteenth = -1;
            clockBeatSent = beat;
            clockStartAt = ! byClock && (! byHost || syncInUse) ? at : -1; // its own clock out, the sync's too (D361)
            auditionKey.store(-1);
            lastAudition = -1;
        }
        else
        {
            sequencer.reset();
            applyPanel(audio->panel, nullptr);
            stepNow.store(-1);
            soundingNow.store(-1);
            clockStopPending = ! playingByClock && (! playingByHost || syncInUse);
            playingByClock = playingByHost = false;
        }
    }

    void Processor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
    {
        const juce::ScopedNoDenormals noDenormals;
        const auto started = juce::Time::getHighResolutionTicks();
        pullFromUi();
        const int numSamples = buffer.getNumSamples();

        // MIDI in: a clock runs the transport; while playing a note transposes the pattern from middle C; while
        // stopped it plays the voice
        for (const auto m : midi)
        {
            const auto msg = m.getMessage();
            const auto at = samplesSoFar + m.samplePosition;
            if (msg.isMidiClock())
                clock.pulse(at);
            else if (msg.isMidiStart())
                clock.start(at);
            else if (msg.isMidiContinue())
                clock.resume();
            else if (msg.isMidiStop())
                clock.stop();
            else if (msg.isSongPositionPointer())
                clock.songPosition(msg.getSongPositionPointerMidiBeat());
            else if (msg.isNoteOn())
            {
                if (playing)
                    transposeRequest.store(juce::jlimit(-12, 24, msg.getNoteNumber() - midiNoteAsWritten));
                else
                    auditionKey.store(juce::jlimit(0, 36, msg.getNoteNumber() - 24));
            }
            else if (msg.isNoteOff() && ! playing && auditionKey.load() == juce::jlimit(0, 36, msg.getNoteNumber() - 24))
                auditionKey.store(-1);
        }
        midi.clear();

        if (const int pr = patternRequest.exchange(-1); pr >= 0)
            sequencer.requestPattern(pr / 2, (pr & 1) != 0);
        sequencer.setTranspose(transposeRequest.load());

        int request = playRequest.exchange(-1);

        // The beat sync (D361), the standalone's: the block's beats at the moment its first sample is heard. It leads
        // through a play head of its own, as a DAW's transport does; Play, Stop and the panel's tempo go to it. Alone
        // in its session, an incoming MIDI clock leads as before; with other peers the sync wins.
        bool bySync = false;
        if (beatSync != nullptr && syncOn.load(std::memory_order_acquire))
        {
            auto& shared = beatSync->audio();
            const std::int64_t now = hostClockForTesting ? hostClockForTesting() : coolsynth::sync::hostMicrosNow();
            const auto t = blockClock.next(now, numSamples);
            const auto* beats = &shared.beginBlock(t.hostMicros + outputLatencyMicros.load(std::memory_order_relaxed), numSamples, t.microsPerSample);
            if (audio->bpm != syncPanelTempo)
            {
                // The panel's tempo changed (or a peer's came back to it, which changes nothing). On the first block
                // of a sync already with others, the session's tempo stands and becomes the panel's (syncTick): the
                // panel's would otherwise win over it, as a change.
                syncPanelTempo = audio->bpm;
                if (syncRunning || beats->peers == 0)
                    beats = &shared.setTempo(syncPanelTempo);
                syncPanelTempoNow.store(syncPanelTempo, std::memory_order_release);
            }
            syncRunning = true;
            bySync = beats->peers > 0 || ! clock.present(samplesSoFar);
            if (bySync)
            {
                // Taking over (switched on, or a clock gone): the sync plays if the pattern plays on its own.
                if (! syncInUse)
                    if (const bool own = playing && ! playingByClock && ! playingByHost; own != shared.isPlaying())
                        beats = own ? &shared.play(0) : &shared.stop(0);
                if (request >= 0 && (request == 1) != shared.isPlaying())
                    beats = request == 1 ? &shared.play(0) : &shared.stop(0);
                request = -1;
            }
            syncHead.beats = *beats;
            syncHead.sampleRate = hostRate;
            syncHead.samples = numSamples;
            syncPeersNow.store(beats->peers, std::memory_order_release);
            syncBpmNow.store(beats->bpm, std::memory_order_release);
        }
        else
            syncRunning = false;
        if (syncInUse && ! bySync)
            playingByHost = false; // the sync switched off while it played: the pattern plays on, on its own
        syncInUse = bySync;

        // In a DAW, its transport runs the sequencer: it plays while the host plays, from where the host is, at its
        // tempo; standalone, the beat sync's play head does the same. Standalone, a MIDI clock does when the sync does
        // not lead: it plays while the clock plays; a clock gone while it played stops it (as the 808's).
        bool byHost = false;
        double hostBeat = 0.0, hostBpm = 0.0;
        if (auto* head = bySync ? &syncHead : wrapperType != wrapperType_Standalone && ! offline ? getPlayHead() : nullptr)
            if (const auto position = head->getPosition())
            {
                if (const auto b = position->getBpm())
                    hostBpm = *b;
                if (const auto ppq = position->getPpqPosition(); ppq && position->getIsPlaying())
                {
                    byHost = true;
                    hostBeat = *ppq; // the sync's count-in may end inside this block: below 0 at its start
                }
            }
        const bool byClock = ! bySync && ! byHost && clock.present(samplesSoFar);
        clockFollowed.store(byClock);
        hostFollowed.store(byHost && ! bySync);
        syncFollowed.store(bySync && syncHead.beats.peers > 0, std::memory_order_release);
        if (bySync && byHost && playing && ! playingByHost)
        {
            playingByHost = true; // the pattern that played on its own goes on led by the sync
            playingByClock = false;
        }
        bool wantPlay = byHost ? true : byClock ? clock.isPlaying() : (request < 0 ? playing : request == 1);
        if (! byClock && playing && playingByClock)
            wantPlay = false;
        if (! byHost && playing && playingByHost)
            wantPlay = false; // the host stopped
        // a chain played once through under a clock stays stopped until that clock stops or starts again
        if (! byClock || ! clock.isPlaying())
            endedUnderClock = false;
        if (byClock && endedUnderClock)
            wantPlay = false;
        double clockBeat = beat;
        blockBpm = juce::jlimit(40.0, 300.0, audio->bpm);
        if (byClock)
            clock.block(samplesSoFar, numSamples, clockBeat, blockBpm);
        if (byHost)
            clockBeat = hostBeat;
        if (hostBpm > 0.0 && (byHost || bySync))
            blockBpm = juce::jlimit(20.0, 400.0, hostBpm); // the sync's even while stopped: the delay's time is the session's
        if (wantPlay != playing)
        {
            // The sync's count-in ends inside this block (a start on the session's bar line, D361): the pattern begins
            // at the sample where the beat reaches 0, as a start at that sample (a beat a hair under 0 is a start at
            // sample 0); the sequencer never sees a beat below 0.
            int at = 0;
            if (wantPlay && bySync && clockBeat < 0.0 && syncHead.beats.beatsPerSample > 0.0)
            {
                at = std::clamp(static_cast<int>(std::ceil(-clockBeat / syncHead.beats.beatsPerSample - 1.0e-3)), 0, numSamples - 1);
                clockBeat = 0.0;
                renderRange(0, at); // stopped until then
            }
            startOrStop(wantPlay, byClock, byHost, clockBeat, at);
            renderRange(at, numSamples);
        }
        else
        {
            if (playing && (playingByClock || playingByHost))
            {
                // where the clock or the host puts this block's start; the host looping back starts the pattern afresh
                if (playingByHost && std::floor(clockBeat * 4.0) < static_cast<double>(sixteenth))
                {
                    sequencer.reset();
                    sixteenth = -1;
                }
                beat = std::max(0.0, clockBeat);
            }
            renderRange(0, numSamples);
        }
        bpmNow.store(bySync ? syncHead.beats.bpm : blockBpm); // the session's tempo, not the block's
        if (playing && sequencer.ended())
        {
            playing = false;
            sequencer.reset();
            stepNow.store(-1);
            soundingNow.store(-1);
            clockStopPending = ! playingByClock;
            endedUnderClock = playingByClock;
            playingByClock = false;
            if (bySync)
            {
                playRequest.store(0); // a chain that does not loop has played through: the sync's Stop, shared
                playingByHost = false;
            }
        }
        playingNow.store(playing);
        countingInNow.store(bySync && syncHead.beats.playing && ! playing, std::memory_order_release);

        stereo.copyFrom(1, 0, stereo, 0, 0, numSamples);
        juce::AudioBuffer<float> view(stereo.getArrayOfWritePointers(), 2, numSamples);
        const auto& panel = audio->panel;
        coolsynth::synth::DelayParametersV2 dp;
        dp.enabled = panel.delayOn;
        dp.timeMs = static_cast<float>(delayBeats(panel.delayDivision) * 60000.0 / juce::jlimit(40.0, 300.0, blockBpm));
        dp.feedback = panel.feedback * coolsynth::synth::GlobalDelay::maxFeedback;
        dp.mix = panel.mix;
        dp.pingPong = false;
        delay->process(view, dp);

        // The output stage (D332): untouched up to -3 dBFS, above it rounded off toward full scale as an analogue
        // stage runs out of voltage, so a pushed Devil Fish never meets the sound card's flat digital clip.
        for (int ch = 0; ch < 2; ++ch)
        {
            float* s = stereo.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
                s[i] = outputStage(s[i]);
        }

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.copyFrom(ch, 0, stereo, std::min(ch, 1), 0, numSamples);

        sendClock(midi, numSamples);

        float peak = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            peak = std::max(peak, std::abs(stereo.getSample(0, i)));
        // The MCP tools' measures: the loudest sample since asked, and the output kept while a capture is armed.
        for (float was = loudest.load(); peak > was && ! loudest.compare_exchange_weak(was, peak);)
        {
        }
        if (captureArmed.load(std::memory_order_acquire))
        {
            const int have = captureCount.load(std::memory_order_relaxed);
            const int take = std::min(numSamples, static_cast<int>(captureBuffer.size()) - have);
            if (take > 0)
            {
                std::copy_n(stereo.getReadPointer(0), take, captureBuffer.data() + have);
                captureCount.store(have + take, std::memory_order_release);
            }
        }
        levelFollow = std::max(static_cast<double>(peak), levelFollow * 0.85);
        levelNow.store(static_cast<float>(levelFollow));
        const double seconds = juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - started);
        loadFollow = 0.9 * loadFollow + 0.1 * (seconds * hostRate / std::max(1, numSamples));
        loadNow.store(static_cast<float>(loadFollow));
        samplesSoFar += numSamples;
    }

    juce::AudioProcessorEditor* Processor::createEditor() { return new Editor(*this); }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new cs303::Processor(); }
