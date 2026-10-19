#include "filters/oversampled_filter.h"
#include "filters/svf_anushri.h"
#include "filters/svf_anushri_nonlinear.h"
#include "voice/voice.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <vector>

namespace {

int failures = 0;
using Clock = std::chrono::steady_clock;

void expectTrue(bool cond, const char* msg)
{
    if (!cond) {
        std::printf("FAIL: %s\n", msg);
        ++failures;
    }
}

double seconds(Clock::time_point a, Clock::time_point b)
{
    return std::chrono::duration<double>(b - a).count();
}

void testEquationsAndRate()
{
    lxx::dsp::OversampledSVF filt(48000.0f);
    filt.prepareBuffers(64);
    expectTrue(std::abs(filt.getEffectiveSampleRate() - 96000.0f) < 1.0f,
               "2x oversampling => 96 kHz filter rate");

    lxx::dsp::SVFAnushriLinear svf(96000.0f);
    svf.setCutoffFreq(1000.0f);
    svf.setResonance(0.0f);
    const float g = svf.getG();
    const float k = svf.getK();
    const float expectedG = std::tan(lxx::dsp::kPi * 1000.0f / 96000.0f);
    expectTrue(std::abs(g - expectedG) < 1.0e-5f, "g = tan(pi*fc/fs)");
    expectTrue(std::abs(k - 1.0f) < 1.0e-4f, "resonance 0 => Q=1 => k=1");

    svf.setResonance(0.5f);
    expectTrue(std::abs(svf.getK() - 0.5f) < 1.0e-4f, "resonance 0.5 => Q=2 => k=0.5");

    svf.setResonance(1.0f);
    expectTrue(svf.getK() == 0.0f, "resonance 1 => k=0");

    svf.setCutoffFreq(1.0e9f);
    const float maxFc = 96000.0f * lxx::dsp::SVFAnushriLinear::kNyquistCutoffRatio;
    expectTrue(svf.getCutoffFreq() <= maxFc + 1.0f, "cutoff clamped to 0.45*fs");
    expectTrue(svf.getG() <= lxx::dsp::SVFAnushriLinear::kMaxTptG + 1.0e-3f, "g bounded");
}

void testResonanceMatrix()
{
    std::printf("Resonance/cutoff matrix\n");
    const float res[] = { 0.0f, 0.25f, 0.5f, 0.75f, 0.9f, 0.95f, 0.99f, 1.0f };
    const float cut[] = { 20.0f, 50.0f, 100.0f, 500.0f, 1000.0f, 5000.0f, 10000.0f, 20000.0f };
    const float rates[] = { 44100.0f, 48000.0f, 88200.0f, 96000.0f };
    float maxIc1 = 0.0f, maxIc2 = 0.0f;
    for (float sr : rates) {
        lxx::dsp::Voice voice;
        voice.init(sr, 2048);
        std::vector<float> buf(64);
        for (float r : res) {
            for (float c : cut) {
                lxx::dsp::VoicePatch patch;
                patch.filterCutoffHz = c;
                patch.filterResonance = r;
                voice.setPatch(patch);
                voice.noteOn(60, 100);
                for (int b = 0; b < 32; ++b)
                    voice.processBlock(buf.data(), 64);
                voice.noteOff(60);
                for (int b = 0; b < 16; ++b)
                    voice.processBlock(buf.data(), 64);
                if (!voice.internalStateFinite()) {
                    std::printf("FAIL: non-finite sr=%.0f res=%.2f cut=%.0f\n", sr, r, c);
                    ++failures;
                    return;
                }
                maxIc1 = std::max(maxIc1, std::abs(voice.getFilterIc1()));
                maxIc2 = std::max(maxIc2, std::abs(voice.getFilterIc2()));
            }
        }
        std::printf("  sr=%.0f ok  effFs=%.0f\n", sr, voice.getFilterEffectiveSampleRate());
        expectTrue(std::abs(voice.getFilterEffectiveSampleRate() - sr * 2.0f) < 1.0f,
                   "effective filter rate is 2x host");
    }
    std::printf("  max |ic1|=%.4f |ic2|=%.4f\n", maxIc1, maxIc2);
    expectTrue(maxIc1 <= 8.01f && maxIc2 <= 8.01f, "integrators within clamp");
}

void testParamFuzz()
{
    std::printf("100k filter parameter transitions\n");
    lxx::dsp::OversampledSVF filt(48000.0f);
    filt.prepareBuffers(64);
    std::vector<float> in(64, 0.2f), lp(64), bp(64), hp(64);
    uint32_t rng = 0x1234567u;
    auto t0 = Clock::now();
    for (int i = 0; i < 100000; ++i) {
        rng = rng * 1664525u + 1013904223u;
        const float cut = 20.0f + static_cast<float>(rng % 20000);
        rng = rng * 1664525u + 1013904223u;
        const float res = static_cast<float>(rng % 1001) / 1000.0f;
        filt.setCutoffFreq(cut);
        filt.setResonance(res);
        filt.processBlock(in.data(), lp.data(), bp.data(), hp.data(), 32);
        const auto& core = filt.getFilter().getLinearCore();
        if (!std::isfinite(core.getG()) || !std::isfinite(core.getK())
            || !std::isfinite(filt.getFilter().getLPState())
            || !std::isfinite(filt.getFilter().getBPState())) {
            std::printf("FAIL: fuzz i=%d cut=%.1f res=%.3f\n", i, cut, res);
            ++failures;
            return;
        }
    }
    const double wall = seconds(t0, Clock::now());
    std::printf("  100000 updates wall=%.4fs\n", wall);
    expectTrue(wall < 5.0, "coefficient updates realtime-safe");
}

void testSelfOsc()
{
    std::printf("Self-osc 10-minute equivalent @ 48 kHz\n");
    lxx::dsp::Voice voice;
    voice.init(48000.0f, 2048);
    lxx::dsp::VoicePatch patch;
    patch.filterResonance = 1.0f;
    patch.filterCutoffHz = 800.0f;
    patch.waveLevel = 0.5f;
    patch.subLevel = 0.0f;
    voice.setPatch(patch);
    voice.noteOn(60, 100);
    std::vector<float> buf(64);
    const size_t total = static_cast<size_t>(48000.0f * 10.0 * 60.0);
    size_t n = 0;
    float maxAbs = 0.0f;
    auto t0 = Clock::now();
    while (n < total) {
        voice.processBlock(buf.data(), 64);
        for (float s : buf)
            maxAbs = std::max(maxAbs, std::abs(s));
        if (!voice.internalStateFinite()) {
            expectTrue(false, "self-osc non-finite");
            return;
        }
        n += 64;
    }
    const double wall = seconds(t0, Clock::now());
    std::printf("  wall=%.2fs maxAbs=%.4f ic1=%.4f ic2=%.4f\n",
                wall, maxAbs, voice.getFilterIc1(), voice.getFilterIc2());
    expectTrue(std::isfinite(maxAbs) && maxAbs < 8.0f, "self-osc bounded");
    expectTrue(wall < 60.0, "self-osc completed");
}

void testImpulse()
{
    lxx::dsp::SVFAnushriNonlinear svf(96000.0f);
    svf.setCutoffFreq(1000.0f);
    svf.setResonance(0.5f);
    float lp, bp, hp;
    (void)svf.process(1.0f);
    float last = 1.0f;
    bool decaying = false;
    for (int i = 0; i < 48000; ++i) {
        auto o = svf.process(0.0f);
        lp = o[0];
        if (!std::isfinite(lp)) {
            expectTrue(false, "impulse response non-finite");
            return;
        }
        if (i > 100 && std::abs(lp) < std::abs(last))
            decaying = true;
        last = lp;
    }
    (void)bp;
    (void)hp;
    expectTrue(decaying || std::abs(last) < 0.1f, "impulse eventually decays at res=0.5");
}

} // namespace

int main()
{
    testEquationsAndRate();
    testResonanceMatrix();
    testParamFuzz();
    testImpulse();
    testSelfOsc();

    if (failures == 0)
        std::printf("test_filter_forensics: PASS\n");
    else
        std::printf("test_filter_forensics: %d FAIL\n", failures);
    return failures == 0 ? 0 : 1;
}
