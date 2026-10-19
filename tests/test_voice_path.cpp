#include "voice/voice.h"
#include "modulation/cv_scaler.h"

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

lxx::dsp::VoicePatch playablePatch()
{
    lxx::dsp::VoicePatch p;
    p.authenticMode = false;
    p.waveLevel = 1.0f;
    p.subLevel = 0.0f;
    p.filterCutoffHz = 8000.0f;
    p.filterResonance = 0.05f;
    p.filterKbdTrack = 0.0f;
    p.filterEnvAmt = 0.0f;
    p.filterLfoAmt = 0.0f;
    p.filterModAmt = 0.0f;
    p.envAttack = 0.001f;
    p.envDecay = 0.2f;
    p.envSustain = 1.0f;
    p.envRelease = 0.05f;
    p.envVcaMorph = 0.0f;
    p.glideTimeSec = 0.0f;
    p.unisonCount = 1;
    p.volume = 1.0f;
    return p;
}

float hfEnergy(const float* x, size_t n, float sr, float minHz)
{
    const int i0 = std::max(1, static_cast<int>(minHz * static_cast<float>(n) / sr));
    const int i1 = static_cast<int>(n / 2) - 1;
    double e = 0.0;
    for (int k = i0; k <= i1; ++k) {
        double re = 0.0, im = 0.0;
        const double w = -2.0 * 3.14159265358979323846 * static_cast<double>(k)
                         / static_cast<double>(n);
        for (size_t i = 0; i < n; ++i) {
            const double a = w * static_cast<double>(i);
            re += static_cast<double>(x[i]) * std::cos(a);
            im += static_cast<double>(x[i]) * std::sin(a);
        }
        e += re * re + im * im;
    }
    return static_cast<float>(e);
}

void testCutoffDirection()
{
    std::printf("VCF cutoff direction\n");
    const float knobs[] = { 0.0f, 0.10f, 0.25f, 0.50f, 0.75f, 0.90f, 1.00f };
    float prevHf = -1.0f;
    float prevHz = -1.0f;
    lxx::dsp::VcfCvScaler scaler;
    float prevDacFreq = scaler.dacToFreq(0.0f);
    expectTrue(scaler.dacToFreq(4095.0f) > prevDacFreq * 2.0f,
               "DAC 4095 opens analog cutoff vs DAC 0");

    for (float n : knobs) {
        const float hz = 20.0f * std::pow(20000.0f / 20.0f, n);
        lxx::dsp::Voice voice;
        voice.init(48000.0f, 2048);
        auto p = playablePatch();
        p.filterCutoffHz = hz;
        p.filterResonance = 0.05f;
        voice.setPatch(p);
        voice.noteOn(60, 120);
        std::vector<float> buf(8192, 0.0f);
        for (size_t i = 0; i < buf.size(); i += 64)
            voice.processBlock(buf.data() + i, 64);
        const float mapped = voice.lastCutoffFrequencyHz();
        const float hf = hfEnergy(buf.data() + 2048, 4096, 48000.0f, 2000.0f);
        std::printf("  knob=%.2f mapped=%.1f Hz hf=%.3e\n", n, mapped, hf);
        expectTrue(std::abs(mapped - hz) / hz < 0.15f, "mapped Hz tracks panel cutoff at C3");
        if (prevHz > 0.0f)
            expectTrue(mapped > prevHz, "cutoff Hz monotonic with knob");
        if (prevHf >= 0.0f)
            expectTrue(hf >= prevHf * 0.85f, "HF energy does not fall as cutoff opens");
        prevHz = mapped;
        prevHf = hf;
        expectTrue(voice.internalStateFinite(), "filter finite during cutoff sweep");
    }
}

void testEnvelopeDirection()
{
    lxx::dsp::Voice fast;
    fast.init(48000.0f, 2048);
    auto p = playablePatch();
    p.envVcaMorph = 0.0f;
    p.envAttack = 0.001f;
    p.envRelease = 0.02f;
    p.envSustain = 1.0f;
    fast.setPatch(p);
    fast.noteOn(60, 120);
    std::vector<float> buf(64);
    float peakFast = 0.0f;
    for (int i = 0; i < 8; ++i) {
        fast.processBlock(buf.data(), buf.size());
        for (float s : buf)
            peakFast = std::max(peakFast, std::abs(s));
    }

    lxx::dsp::Voice slow;
    slow.init(48000.0f, 2048);
    p.envAttack = 0.25f;
    slow.setPatch(p);
    slow.noteOn(60, 120);
    float peakSlow = 0.0f;
    for (int i = 0; i < 8; ++i) {
        slow.processBlock(buf.data(), buf.size());
        for (float s : buf)
            peakSlow = std::max(peakSlow, std::abs(s));
    }
    std::printf("VCA attack 1ms peak=%.4f  250ms peak=%.4f\n", peakFast, peakSlow);
    expectTrue(peakFast > peakSlow * 1.5f, "longer attack reaches less amplitude in same time");
}

void testIdleAfterRelease()
{
    lxx::dsp::Voice voice;
    voice.init(48000.0f, 2048);
    auto p = playablePatch();
    p.envRelease = 0.02f;
    voice.setPatch(p);
    voice.noteOn(60, 100);
    std::vector<float> buf(64);
    for (int i = 0; i < 32; ++i)
        voice.processBlock(buf.data(), buf.size());
    voice.noteOff(60);
    for (int i = 0; i < 256; ++i)
        voice.processBlock(buf.data(), buf.size());
    expectTrue(voice.isIdle(), "voice idle after release");
    float peak = 0.0f;
    voice.processBlock(buf.data(), buf.size());
    for (float s : buf)
        peak = std::max(peak, std::abs(s));
    expectTrue(peak == 0.0f, "idle output silence");
}

void testEnvOpensFilter()
{
    lxx::dsp::Voice voice;
    voice.init(48000.0f, 2048);
    auto p = playablePatch();
    p.filterCutoffHz = 400.0f;
    p.filterEnvAmt = 0.0f;
    p.envSustain = 1.0f;
    voice.setPatch(p);
    voice.noteOn(60, 120);
    std::vector<float> buf(64);
    for (int i = 0; i < 16; ++i)
        voice.processBlock(buf.data(), buf.size());
    const float closed = voice.lastCutoffFrequencyHz();
    p.filterEnvAmt = 1.0f;
    voice.setPatch(p);
    for (int i = 0; i < 16; ++i)
        voice.processBlock(buf.data(), buf.size());
    const float opened = voice.lastCutoffFrequencyHz();
    std::printf("VCF env 0 -> %.1f Hz  env 1 -> %.1f Hz\n", closed, opened);
    expectTrue(opened > closed * 1.5f, "+env amount opens cutoff");
}

void testCutoffSweepMatrix()
{
    std::printf("VCF cutoff sweep matrix\n");
    const float knobs[] = {
        0.00f, 0.05f, 0.10f, 0.20f, 0.30f, 0.40f, 0.50f,
        0.60f, 0.70f, 0.80f, 0.90f, 0.95f, 1.00f
    };
    float prev = -1.0f;
    for (float n : knobs) {
        const float hz = 20.0f * std::pow(20000.0f / 20.0f, n);
        lxx::dsp::Voice voice;
        voice.init(48000.0f, 2048);
        auto p = playablePatch();
        p.filterCutoffHz = hz;
        voice.setPatch(p);
        voice.noteOn(60, 100);
        std::vector<float> buf(64);
        voice.processBlock(buf.data(), buf.size());
        const float mapped = voice.lastCutoffFrequencyHz();
        std::printf("  n=%.2f mapped=%.1f Hz finite=%d\n",
                    n, mapped, voice.internalStateFinite() ? 1 : 0);
        expectTrue(std::isfinite(mapped), "mapped cutoff finite");
        if (prev > 0.0f)
            expectTrue(mapped > prev, "sweep monotonic");
        expectTrue(voice.internalStateFinite(), "voice finite during sweep");
        prev = mapped;
    }
}

void testVoiceParamFuzz()
{
    std::printf("100k VCO/VCF/VCA parameter transitions\n");
    lxx::dsp::Voice voice;
    voice.init(48000.0f, 2048);
    std::vector<float> buf(32, 0.0f);
    uint32_t rng = 0xC0FFEEu;
    auto next = [&rng]() {
        rng = rng * 1664525u + 1013904223u;
        return rng;
    };
    int nan = 0;
    int inf = 0;
    voice.noteOn(60, 100);
    for (int i = 0; i < 100000; ++i) {
        auto p = playablePatch();
        p.oscRange = static_cast<float>(static_cast<int>(next() % 5) - 2);
        p.oscFine = (static_cast<float>(next() % 1001) / 1000.0f) * 100.0f - 50.0f;
        p.waveLevel = static_cast<float>(next() % 1001) / 1000.0f;
        p.subLevel = static_cast<float>(next() % 1001) / 1000.0f;
        p.filterCutoffHz = 20.0f * std::pow(1000.0f, static_cast<float>(next() % 1001) / 1000.0f);
        p.filterResonance = static_cast<float>(next() % 1001) / 1000.0f;
        p.filterEnvAmt = static_cast<float>(next() % 1001) / 1000.0f;
        p.filterLfoAmt = static_cast<float>(next() % 1001) / 1000.0f;
        p.filterKbdTrack = static_cast<float>(next() % 201) / 100.0f;
        p.envAttack = 0.001f + static_cast<float>(next() % 1000) / 1000.0f;
        p.envDecay = 0.001f + static_cast<float>(next() % 1000) / 1000.0f;
        p.envSustain = static_cast<float>(next() % 1001) / 1000.0f;
        p.envRelease = 0.001f + static_cast<float>(next() % 1000) / 1000.0f;
        p.envVcaMorph = static_cast<float>(next() % 1001) / 1000.0f;
        p.volume = 0.2f + 0.8f * static_cast<float>(next() % 1001) / 1000.0f;
        p.authenticMode = (next() & 1u) != 0;
        voice.setPatch(p);
        if ((i % 17) == 0)
            voice.noteOn(static_cast<uint8_t>(36 + (next() % 48)), 100);
        if ((i % 19) == 0)
            voice.noteOff(static_cast<uint8_t>(36 + (next() % 48)));
        voice.processBlock(buf.data(), buf.size());
        for (float s : buf) {
            if (!std::isfinite(s)) {
                if (s != s)
                    ++nan;
                else
                    ++inf;
            }
        }
        if (!voice.internalStateFinite()) {
            std::printf("FAIL: non-finite internal state at i=%d\n", i);
            ++failures;
            return;
        }
    }
    std::printf("  transitions=100000 nan=%d inf=%d\n", nan, inf);
    expectTrue(nan == 0 && inf == 0, "fuzz host output finite");
}

} // namespace

int main()
{
    testCutoffDirection();
    testEnvelopeDirection();
    testIdleAfterRelease();
    testEnvOpensFilter();
    testCutoffSweepMatrix();
    testVoiceParamFuzz();
    if (failures == 0)
        std::printf("test_voice_path: PASS\n");
    else
        std::printf("test_voice_path: FAIL (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}
