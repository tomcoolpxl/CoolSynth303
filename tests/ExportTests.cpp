// The WAV export (CoolSynth's D355): its lengths, the ring-out, the dry voice, the seamless loop, 96 kHz, the file.

#include <cmath>
#include <memory>

#include <juce_audio_formats/juce_audio_formats.h>

#include "AudioExport.h"

namespace
{
    using namespace cs303;

    ExportJob jobOf(ExportEnd end, int times = 1, bool dry = false, double rate = 48000.0)
    {
        ExportJob job;
        job.project.bpm = 120.0; // a sixteen-step pattern takes 2 s
        job.end = end;
        job.times = times;
        job.dry = dry;
        job.sampleRate = rate;
        return job;
    }

    bool exportOf(const ExportJob& job, ExportResult& result)
    {
        std::atomic<float> done { 0.0f };
        std::atomic<bool> cancel { false };
        return renderExport(job, result, done, cancel) && done.load() == 1.0f;
    }

    double rmsOf(const juce::AudioBuffer<float>& a, int from, int to)
    {
        double s = 0.0;
        for (int i = from; i < to; ++i)
            s += static_cast<double>(a.getSample(0, i)) * a.getSample(0, i);
        return std::sqrt(s / std::max(1, to - from));
    }

    bool sameSamples(const juce::AudioBuffer<float>& a, int aFrom, const juce::AudioBuffer<float>& b, int bFrom, int count)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < count; ++i)
                if (a.getSample(c, aFrom + i) != b.getSample(c, bFrom + i))
                    return false;
        return true;
    }

    // A class of its own, its renders on the heap (the runner's frames overflow in a Debug build otherwise).
    class ExportTests final : public juce::UnitTest
    {
    public:
        ExportTests() : juce::UnitTest("Export", "CoolSynth303") {}

        void runTest() override
        {
            beginTest("a_pattern_or_the_chain_is_as_long_as_its_steps");
            {
                auto job = std::make_unique<ExportJob>(jobOf(ExportEnd::cut, 2));
                expectEquals(stepsPerPass(*job), 16);
                expectEquals(static_cast<int>(loopSamples(*job)), 192000, "two times 16 steps at 120 BPM, 48 kHz");
                job->project.pattern(1, false).length = 8;
                job->project.chain = { { 0, false, 2 }, { 1, false, 1 } };
                job->chain = true;
                expectEquals(stepsPerPass(*job), 40);
                job->sampleRate = 96000.0;
                expectEquals(static_cast<int>(loopSamples(*job)), 2 * 96000 * 5, "40 steps twice: 20 s at 96 kHz");
            }

            beginTest("cut_it_ends_where_the_times_end_and_sounds");
            {
                auto result = std::make_unique<ExportResult>();
                expect(exportOf(jobOf(ExportEnd::cut, 2), *result));
                expectEquals(result->mix.getNumSamples(), 192000);
                expectEquals(result->mix.getNumChannels(), 2);
                expect(rmsOf(result->mix, 0, 192000) > 0.01, "it sounds");
                expectEquals(result->dry.getNumSamples(), 0, "no dry voice unless asked");
            }

            beginTest("ringing_on_it_ends_once_quiet_and_the_dry_voice_lines_up");
            {
                auto result = std::make_unique<ExportResult>();
                expect(exportOf(jobOf(ExportEnd::ring, 1, true), *result));
                const int n = result->mix.getNumSamples();
                expect(n > 96000 + 4800, "the delay rings on past the end");
                expect(n < 96000 + 20 * 48000);
                expectEquals(result->dry.getNumSamples(), n, "the dry voice as long as the mix");
                expect(rmsOf(result->mix, n - 2400, n) < 1.0e-4, "quiet at its end");
                // the dry voice is the mix as Drive and Delay off play it, and not the mix
                expect(! sameSamples(result->mix, 0, result->dry, 0, 96000), "the mix has Drive and Delay");
                auto plain = std::make_unique<ExportJob>(jobOf(ExportEnd::ring));
                plain->project.panel.driveOn = plain->project.panel.delayOn = false;
                auto plainResult = std::make_unique<ExportResult>();
                expect(exportOf(*plain, *plainResult));
                expect(sameSamples(result->dry, 0, plainResult->mix, 0, std::min(n, plainResult->mix.getNumSamples())),
                       "the dry voice is the mix with Drive and Delay off");
            }

            beginTest("with_drive_and_delay_off_the_dry_voice_is_the_mix");
            {
                auto job = std::make_unique<ExportJob>(jobOf(ExportEnd::cut, 1, true));
                job->project.panel.driveOn = job->project.panel.delayOn = false;
                auto result = std::make_unique<ExportResult>();
                expect(exportOf(*job, *result));
                expect(sameSamples(result->mix, 0, result->dry, 0, result->mix.getNumSamples()), "the same samples");
            }

            beginTest("a_seamless_loop_is_the_time_kept_after_the_loop_has_played_round");
            {
                // 2 s a time through: played round five times (10 s), the sixth kept; a cut render of six times ends
                // with the same samples.
                auto loop = std::make_unique<ExportResult>();
                auto cut = std::make_unique<ExportResult>();
                expect(exportOf(jobOf(ExportEnd::loop, 1), *loop));
                expect(exportOf(jobOf(ExportEnd::cut, 6), *cut));
                expectEquals(loop->mix.getNumSamples(), 96000);
                expect(sameSamples(loop->mix, 0, cut->mix, 5 * 96000, 96000), "the sixth time through");
                // its start holds the delay ringing from the end; a cut render's first time does not
                expect(! sameSamples(loop->mix, 0, cut->mix, 0, 4800), "the ring-out folded into the start");
            }

            beginTest("at_96_khz_the_voice_runs_at_its_own_rate");
            {
                auto result = std::make_unique<ExportResult>();
                expect(exportOf(jobOf(ExportEnd::cut, 1, false, 96000.0), *result));
                expectEquals(result->mix.getNumSamples(), 192000);
                expect(rmsOf(result->mix, 0, 192000) > 0.01, "it sounds");
            }

            beginTest("it_writes_a_24_bit_wav_and_the_dry_file_beside_it");
            {
                auto result = std::make_unique<ExportResult>();
                expect(exportOf(jobOf(ExportEnd::cut, 1), *result));
                const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("CoolSynth303ExportTest");
                folder.createDirectory();
                const auto file = folder.getChildFile("Acid.wav");
                expect(writeWav24(file, result->mix, 48000.0));
                expectEquals(dryFile(file).getFileName(), juce::String("Acid - dry.wav"));
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(file.createInputStream().release(), true));
                expect(reader != nullptr);
                if (reader != nullptr)
                {
                    expectEquals(static_cast<int>(reader->bitsPerSample), 24);
                    expectEquals(reader->sampleRate, 48000.0);
                    expectEquals(static_cast<int>(reader->lengthInSamples), result->mix.getNumSamples());
                }
                reader.reset();
                folder.deleteRecursively();
            }
        }
    };

    static ExportTests exportTests;
}
