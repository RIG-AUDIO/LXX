#include "drums/drum_synth.h"
#include "drums/drum_patch.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

int failures = 0;

void expectTrue(bool cond, const char* msg)
{
    if (!cond) {
        std::printf("FAIL: %s\n", msg);
        ++failures;
    }
}

float dftPeakHz(const float* x, size_t n, float sampleRate, float minHz, float maxHz)
{
    const int i0 = std::max(1, static_cast<int>(minHz * static_cast<float>(n) / sampleRate));
    const int i1 = std::min(static_cast<int>(n / 2) - 2,
                            static_cast<int>(maxHz * static_cast<float>(n) / sampleRate));
    double best = 0.0;
    int bestBin = i0;
    for (int k = i0; k <= i1; ++k) {
        double re = 0.0, im = 0.0;
        const double w = -2.0 * 3.14159265358979323846 * static_cast<double>(k)
                         / static_cast<double>(n);
        for (size_t i = 0; i < n; ++i) {
            const double a = w * static_cast<double>(i);
            re += static_cast<double>(x[i]) * std::cos(a);
            im += static_cast<double>(x[i]) * std::sin(a);
        }
        const double mag = re * re + im * im;
        if (mag > best) {
            best = mag;
            bestBin = k;
        }
    }
    return static_cast<float>(bestBin) * sampleRate / static_cast<float>(n);
}

float durationSeconds(const float* x, size_t n, float sampleRate, float thresh)
{
    int last = 0;
    for (size_t i = 0; i < n; ++i) {
        if (std::abs(x[i]) > thresh)
            last = static_cast<int>(i);
    }
    return static_cast<float>(last) / sampleRate;
}

struct RateResult {
    float peakHz = 0.0f;
    float duration = 0.0f;
};

RateResult renderDrum(float sr, uint8_t instrument)
{
    lxx::dsp::DrumSynth synth;
    synth.init(sr);
    synth.seedRandom(0x21);
    synth.morphPatch(instrument, 0);
    synth.setBandwidth(255);
    synth.trigger(instrument, 200);

    const size_t n = static_cast<size_t>(sr * 0.35f);
    std::vector<float> buf(n, 0.0f);
    const size_t block = 64;
    for (size_t i = 0; i < n; i += block)
        synth.processBlock(buf.data() + i, std::min(block, n - i));

    RateResult r;
    r.peakHz = dftPeakHz(buf.data(), std::min<size_t>(n, 4096), sr, 30.0f, 800.0f);
    r.duration = durationSeconds(buf.data(), n, sr, 0.02f);
    return r;
}

float nativeFreqFromIncrement(float sr, uint8_t instrument)
{
    lxx::dsp::DrumSynth synth;
    synth.init(sr);
    synth.seedRandom(0x21);
    synth.morphPatch(instrument, 0);
    synth.setBandwidth(255);
    synth.trigger(instrument, 200);
    const size_t n = static_cast<size_t>(sr * 0.02f);
    std::vector<float> buf(n, 0.0f);
    for (size_t i = 0; i < n; i += 32)
        synth.processBlock(buf.data() + i, std::min<size_t>(32, n - i));
    const float incHz = synth.debugOscillatorHz(instrument);
    return incHz;
}

} // namespace

int main()
{
    std::printf("ORIGINAL/REFERENCE DRUM RATE: %.3f Hz (20e6/510)\n",
                lxx::dsp::kFirmwareAudioRate);
    std::printf("44.1 kHz is NOT the native firmware clock.\n");

    const float rates[] = { 44100.0f, 48000.0f, 88200.0f, 96000.0f };
    const char* names[] = { "BD", "SD", "HH" };

    for (uint8_t inst = 0; inst < 3; ++inst) {
        std::printf("ENGINE %s\n", names[inst]);
        RateResult ref[4];
        float nativeHz[4] = {};
        for (int i = 0; i < 4; ++i) {
            ref[i] = renderDrum(rates[i], inst);
            nativeHz[i] = nativeFreqFromIncrement(rates[i], inst);
            std::printf("  %.0f  peak=%.1f Hz  native=%.1f Hz  dur=%.3fs\n",
                        rates[i], ref[i].peakHz, nativeHz[i], ref[i].duration);
        }
        for (int i = 1; i < 4; ++i) {
            const float pitchRatio = nativeHz[i] / std::max(nativeHz[0], 1.0f);
            const float durRatio = ref[i].duration / std::max(ref[0].duration, 0.001f);
            std::printf("  vs 44.1: native pitch ratio=%.4f dur ratio=%.3f\n", pitchRatio, durRatio);
            if (inst < 2) {
                expectTrue(pitchRatio > 0.97f && pitchRatio < 1.03f,
                           "drum native pitch sample-rate invariant");
            } else {
                expectTrue(pitchRatio > 0.97f && pitchRatio < 1.03f,
                           "HH native pitch sample-rate invariant");
            }
            expectTrue(durRatio > 0.90f && durRatio < 1.12f,
                       "drum duration sample-rate invariant");
        }
    }

    if (failures == 0)
        std::printf("test_drums_samplerate: PASS\n");
    else
        std::printf("test_drums_samplerate: FAIL (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}
