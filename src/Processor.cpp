#include "Processor.h"

#include <cmath>

#include "Editor.h"
#include "synth/GlobalDelay.h"
#include "synth/HalfBand2x.h"
#include "synth/v3/ShaperProcessors.h"
#include "synth/v3/Tb303Voice.h"

namespace cs303
{
    using coolsynth::synth::v3::Tb303Voice;
    using coolsynth::synth::v3::DriveProcessor;

    namespace
    {
        constexpr int maxUndo = 64;
        constexpr double dcCornerHz = 8.0; // C38 into the mixer's input: the output's DC taken off
        constexpr int midiNoteAsWritten = 60; // a key or MIDI note at middle C plays the pattern as written

        /** The Volume knob: silent at 0, unity near three quarters, +6 dB at the top. */
        double volumeGain(double v) noexcept { return 2.0 * v * v * v; }
    }

    Processor::Processor()
        : juce::AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
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
        push();
    }

    Processor::~Processor() = default;

    bool Processor::isBusesLayoutSupported(const BusesLayout& layouts) const
    {
        return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
               || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
    }

    void Processor::prepareToPlay(double sampleRate, int samplesPerBlock)
    {
        hostRate = sampleRate > 0.0 ? sampleRate : 48000.0;
        factor = hostRate < 80000.0 ? 2 : 1; // the voice near 96 kHz (D294): twice 44.1 or 48 kHz, a fast device's own rate
        voiceRate = factor * hostRate;
        const int block = std::max(samplesPerBlock, maxBlock);
        voiceBuffer.assign(static_cast<size_t>(2 * block), 0.0f);
        driveScratch.assign(static_cast<size_t>(2 * block), 0.0f);
        stereo.setSize(2, block, false, true, true);
        for (auto* v : { voice.get(), fishVoice.get() })
        {
            v->prepare(voiceRate);
            v->settle(noteVolts(12));
        }
        decimator->reset();
        coolsynth::synth::v3::SlotMemory memory;
        drive->prepare(voiceRate, block, memory);
        delay->prepare(hostRate, block, 2);
        clock.prepare(hostRate);
        samplesSoFar = 0;
        playingByClock = endedUnderClock = false;
        sequencer.reset();
        dcState = dcIn = 0.0;
        playing = false;
        sixteenth = -1;
        dirty.store(true);
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
        voice->setKnobs(k);
        fishVoice->setKnobs(k);
    }

    void Processor::renderSpan(float* out, int start, int count) noexcept
    {
        const auto& panel = audio->panel;
        const double bpm = juce::jlimit(20.0, 400.0, blockBpm);
        const double beatPerVoiceSample = bpm / 60.0 / voiceRate;
        const double secondsPerStep = 15.0 / bpm;
        float* v = voiceBuffer.data();
        const int n = factor * count;
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
        // core for nothing); the next note settles it again first.
        const bool wanted = playing || key >= 0;
        if (! wanted && quietVoiceSamples > static_cast<std::int64_t>(restAfterSeconds * voiceRate))
        {
            resting = true;
            std::fill(v, v + n, 0.0f);
            dcIn = dcState = 0.0;
        }
        else
        {
            if (resting)
            {
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
            double y = active->process(volts, gate, accent, slide);
            if (! std::isfinite(y))
            {
                active->prepare(voiceRate);
                active->settle(volts);
                y = 0.0;
            }
            // C38: the output's DC off, a one-pole high-pass
            const double r = 1.0 - juce::MathConstants<double>::twoPi * dcCornerHz / voiceRate;
            const double hp = y - dcIn + r * dcState;
            dcIn = y;
            dcState = hp;
            v[i] = static_cast<float>(hp * outputScale);
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
        const double gainTarget = volumeGain(panel.volume);
        const double gainStep = (gainTarget - gainNow) / std::max(1, count);
        for (int i = 0; i < count; ++i)
        {
            gainNow += gainStep;
            const float y = factor == 2 ? decimator->process(v[2 * i], v[2 * i + 1]) : v[i];
            out[start + i] = static_cast<float>(y * gainNow);
        }
    }

    void Processor::sendClock(juce::MidiBuffer& midi, int numSamples) noexcept
    {
        if (clockStartPending)
        {
            midi.addEvent(juce::MidiMessage::midiStart(), 0);
            clockStartPending = false;
        }
        if (clockStopPending)
        {
            midi.addEvent(juce::MidiMessage::midiStop(), 0);
            clockStopPending = false;
        }
        if (! playing || playingByClock || playingByHost)
            return; // following a clock or a host, it does not send one of its own
        const double perSample = juce::jlimit(20.0, 400.0, blockBpm) / 60.0 / hostRate;
        const double startBeat = beat - perSample * numSamples; // the block's start: beat has moved on in renderSpan
        for (int i = 0; i < numSamples; ++i)
        {
            const double b = startBeat + perSample * i;
            if (b >= clockBeatSent)
            {
                midi.addEvent(juce::MidiMessage::midiClock(), i);
                clockBeatSent += 1.0 / 24.0;
            }
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

        // In a DAW, its transport runs the sequencer: it plays while the host plays, from where the host is, at its
        // tempo. Standalone, a MIDI clock does: it plays while the clock plays; a clock gone while it played stops it
        // (as the 808's).
        bool byHost = false;
        double hostBeat = 0.0, hostBpm = 0.0;
        if (wrapperType != wrapperType_Standalone)
            if (auto* head = getPlayHead())
                if (const auto position = head->getPosition())
                {
                    if (const auto b = position->getBpm())
                        hostBpm = *b;
                    if (const auto ppq = position->getPpqPosition(); ppq && position->getIsPlaying())
                    {
                        byHost = true;
                        hostBeat = *ppq;
                    }
                }
        const bool byClock = ! byHost && clock.present(samplesSoFar);
        clockFollowed.store(byClock);
        hostFollowed.store(byHost);
        const int request = playRequest.exchange(-1);
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
        {
            clockBeat = hostBeat;
            if (hostBpm > 0.0)
                blockBpm = juce::jlimit(20.0, 400.0, hostBpm);
        }
        if (wantPlay != playing)
        {
            playing = wantPlay;
            if (playing)
            {
                playingByClock = byClock;
                playingByHost = byHost;
                sequencer.reset();
                beat = byClock || byHost ? clockBeat : 0.0;
                sixteenth = -1;
                clockBeatSent = beat;
                clockStartPending = ! byClock && ! byHost;
                auditionKey.store(-1);
                lastAudition = -1;
            }
            else
            {
                sequencer.reset();
                applyPanel(audio->panel, nullptr);
                stepNow.store(-1);
                soundingNow.store(-1);
                clockStopPending = ! playingByClock && ! playingByHost;
                playingByClock = playingByHost = false;
            }
        }
        else if (playing && (playingByClock || playingByHost))
        {
            // where the clock or the host puts this block's start; the host looping back starts the pattern afresh
            if (playingByHost && std::floor(clockBeat * 4.0) < static_cast<double>(sixteenth))
            {
                sequencer.reset();
                sixteenth = -1;
            }
            beat = clockBeat;
        }
        bpmNow.store(blockBpm);
        for (int done = 0; done < numSamples;)
        {
            const int span = std::min(maxBlock, numSamples - done);
            renderSpan(stereo.getWritePointer(0), done, span);
            done += span;
        }
        if (playing && sequencer.ended())
        {
            playing = false;
            sequencer.reset();
            stepNow.store(-1);
            soundingNow.store(-1);
            clockStopPending = ! playingByClock;
            endedUnderClock = playingByClock;
            playingByClock = false;
        }
        playingNow.store(playing);

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

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.copyFrom(ch, 0, stereo, std::min(ch, 1), 0, numSamples);

        sendClock(midi, numSamples);

        float peak = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            peak = std::max(peak, std::abs(stereo.getSample(0, i)));
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
