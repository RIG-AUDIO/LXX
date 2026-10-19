#include "drums/drum_synth.h"
#include "filters/oversampled_filter.h"
#include "filters/svf_anushri.h"
#include "filters/svf_anushri_nonlinear.h"
#include "oscillators/blep_oscillator.h"
#include "voice/voice.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

double seconds(Clock::time_point a, Clock::time_point b)
{
    return std::chrono::duration<double>(b - a).count();
}

void report(const char* name, double audioSec, double wallSec, size_t samples, int blocks)
{
    const double rt = (audioSec > 0.0) ? (wallSec / audioSec) : 0.0;
    const double usBlock = (blocks > 0) ? (wallSec * 1.0e6 / static_cast<double>(blocks)) : 0.0;
    std::printf("%-28s audio=%.3fs wall=%.4fs rt=%.3fx us/block=%.1f samples=%zu\n",
                name, audioSec, wallSec, rt, usBlock, samples);
}

}  // namespace

int main()
{
    constexpr float kSr = 48000.0f;
    constexpr int kBlock = 64;
    constexpr double kAudio = 2.0;
    const size_t total = static_cast<size_t>(kSr * kAudio);
    const int nBlocks = static_cast<int>((total + kBlock - 1) / kBlock);

    std::vector<float> buf(static_cast<size_t>(kBlock));

    {
        lxx::dsp::SVFAnushriLinear svf(kSr * 2.0f);
        svf.setResonance(0.35f);
        auto t0 = Clock::now();
        for (int i = 0; i < 1500; ++i)
            svf.setCutoffFreq(400.0f + static_cast<float>(i));
        auto t1 = Clock::now();
        std::printf("%-28s wall=%.4fs  (1500 cutoff updates)\n",
                    "SVF setCutoffFreq x1500", seconds(t0, t1));
    }

    {
        lxx::dsp::OversampledSVF filt(kSr);
        filt.prepareBuffers(2048);
        filt.setCutoffFreq(4000.0f);
        filt.setResonance(0.35f);
        std::vector<float> in(kBlock, 0.1f), lp(kBlock), bp(kBlock), hp(kBlock);
        auto t0 = Clock::now();
        for (int b = 0; b < nBlocks; ++b)
            filt.processBlock(in.data(), lp.data(), bp.data(), hp.data(), kBlock);
        auto t1 = Clock::now();
        report("OversampledSVF process", kAudio, seconds(t0, t1), total, nBlocks);
    }

    {
        lxx::dsp::BlePOscillator osc(kSr);
        osc.setWaveform(lxx::dsp::Waveform::TRIANGLE);
        osc.setFrequency(440.0f);
        auto t0 = Clock::now();
        for (int b = 0; b < nBlocks; ++b)
            for (int i = 0; i < kBlock; ++i)
                buf[static_cast<size_t>(i)] = osc.process();
        auto t1 = Clock::now();
        report("BLEP triangle", kAudio, seconds(t0, t1), total, nBlocks);
    }

    {
        lxx::dsp::Voice voice;
        voice.init(kSr, 2048);
        auto t0 = Clock::now();
        for (int b = 0; b < nBlocks; ++b)
            voice.processBlock(buf.data(), kBlock);
        auto t1 = Clock::now();
        report("Voice idle", kAudio, seconds(t0, t1), total, nBlocks);
    }

    {
        lxx::dsp::Voice voice;
        voice.init(kSr, 2048);
        voice.noteOn(60, 100);
        auto t0 = Clock::now();
        for (int b = 0; b < nBlocks; ++b)
            voice.processBlock(buf.data(), kBlock);
        auto t1 = Clock::now();
        report("Voice one note", kAudio, seconds(t0, t1), total, nBlocks);
    }

    {
        lxx::dsp::Voice voice;
        voice.init(kSr, 2048);
        auto patch = voice.getPatch();
        patch.filterResonance = 0.95f;
        patch.filterCutoffHz = 800.0f;
        patch.waveLevel = 1.0f;
        patch.subLevel = 1.0f;
        patch.unisonCount = 4;
        voice.setPatch(patch);
        voice.noteOn(36, 127);
        auto t0 = Clock::now();
        for (int b = 0; b < nBlocks; ++b)
            voice.processBlock(buf.data(), kBlock);
        auto t1 = Clock::now();
        report("Voice worst patch", kAudio, seconds(t0, t1), total, nBlocks);
    }

    auto benchVoice = [&](float sr, const char* tag, bool idle, bool worst, bool rapidMod) {
        const size_t nTotal = static_cast<size_t>(sr * kAudio);
        const int blocks = static_cast<int>((nTotal + kBlock - 1) / kBlock);
        lxx::dsp::Voice voice;
        voice.init(sr, 2048);
        auto patch = voice.getPatch();
        if (worst) {
            patch.filterResonance = 0.95f;
            patch.filterCutoffHz = 800.0f;
            patch.waveLevel = 1.0f;
            patch.subLevel = 1.0f;
            patch.unisonCount = 4;
            voice.setPatch(patch);
        }
        if (!idle)
            voice.noteOn(worst ? 36 : 60, 127);
        auto t0 = Clock::now();
        for (int b = 0; b < blocks; ++b) {
            if (rapidMod) {
                patch.filterCutoffHz = 80.0f + static_cast<float>((b * 37) % 12000);
                patch.filterLfoAmt = 0.8f;
                voice.setPatch(patch);
            }
            voice.processBlock(buf.data(), kBlock);
        }
        auto t1 = Clock::now();
        report(tag, kAudio, seconds(t0, t1), nTotal, blocks);
    };

    benchVoice(48000.0f, "48k idle", true, false, false);
    benchVoice(48000.0f, "48k one note", false, false, false);
    benchVoice(48000.0f, "48k res high", false, true, false);
    benchVoice(48000.0f, "48k rapid mod", false, true, true);
    benchVoice(96000.0f, "96k idle", true, false, false);
    benchVoice(96000.0f, "96k one note", false, false, false);
    benchVoice(96000.0f, "96k res high", false, true, false);
    benchVoice(96000.0f, "96k rapid mod", false, true, true);

    {
        lxx::dsp::DrumSynth drums;
        drums.init(48000.0f);
        drums.seedRandom(0x21);
        drums.morphPatch(0, 128);
        drums.morphPatch(1, 128);
        drums.morphPatch(2, 128);
        drums.setBandwidth(255);
        auto t0 = Clock::now();
        for (int b = 0; b < nBlocks; ++b) {
            if ((b % 16) == 0)
                drums.trigger(static_cast<uint8_t>(b % 3), 200);
            drums.processBlock(buf.data(), static_cast<size_t>(kBlock));
        }
        auto t1 = Clock::now();
        report("Drums 48k active", kAudio, seconds(t0, t1), total, nBlocks);
    }

    {
        const float sr = 96000.0f;
        const size_t nTotal = static_cast<size_t>(sr * kAudio);
        const int blocks = static_cast<int>((nTotal + kBlock - 1) / kBlock);
        lxx::dsp::DrumSynth drums;
        drums.init(sr);
        drums.seedRandom(0x21);
        drums.setBandwidth(255);
        auto t0 = Clock::now();
        for (int b = 0; b < blocks; ++b) {
            if ((b % 16) == 0)
                drums.trigger(static_cast<uint8_t>(b % 3), 200);
            drums.processBlock(buf.data(), static_cast<size_t>(kBlock));
        }
        auto t1 = Clock::now();
        report("Drums 96k active", kAudio, seconds(t0, t1), nTotal, blocks);
    }

    return 0;
}
