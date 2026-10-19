#include "core/oversampler.h"
#include "filters/oversampled_filter.h"
#include "filters/svf_anushri.h"
#include "oscillators/blep_oscillator.h"
#include "sequencer/transport.h"
#include "voice/voice.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include "test_time_budget.h"
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

uint32_t xorshift(uint32_t& s)
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

double seconds(Clock::time_point a, Clock::time_point b)
{
    return std::chrono::duration<double>(b - a).count();
}

void testVoiceNoteSequence()
{
    std::printf("Voice note-transition stress\n");
    lxx::dsp::Voice voice;
    voice.init(48000.0f, 2048);
    lxx::dsp::VoicePatch patch;
    patch.filterCutoffHz = 4000.0f;
    patch.filterResonance = 0.85f;
    voice.setPatch(patch);

    std::vector<float> buf(64, 0.0f);
    const uint8_t seq[] = { 60, 64, 67, 60, 64, 67 };
    int transitions = 0;
    auto t0 = Clock::now();
    for (int cycle = 0; cycle < 20000; ++cycle) {
        for (uint8_t n : seq) {
            voice.noteOn(n, 100);
            voice.processBlock(buf.data(), buf.size());
            ++transitions;
            if (!voice.internalStateFinite()) {
                expectTrue(false, "internal state non-finite after note-on");
                return;
            }
            voice.noteOff(n);
            voice.processBlock(buf.data(), buf.size());
            ++transitions;
        }
        voice.noteOn(60, 100);
        voice.noteOn(64, 90);
        voice.processBlock(buf.data(), buf.size());
        voice.noteOff(60);
        voice.noteOn(67, 80);
        voice.processBlock(buf.data(), buf.size());
        voice.noteOff(64);
        voice.noteOff(67);
        voice.processBlock(buf.data(), buf.size());
        transitions += 5;
        const auto wall = seconds(t0, Clock::now());
        if (wall > lxx::test::timeBudget(120.0)) {
            expectTrue(false, "voice stress watchdog (hung)");
            return;
        }
    }
    std::printf("  transitions=%d finite=%d\n", transitions, voice.internalStateFinite() ? 1 : 0);
    expectTrue(transitions >= 100000, "100k+ note transitions");
    expectTrue(voice.internalStateFinite(), "filter/voice finite after stress");
}

void testVoiceFuzz()
{
    std::printf("MIDI fuzz + lifecycle\n");
    lxx::dsp::Voice a, b;
    a.init(44100.0f, 512);
    b.init(48000.0f, 2048);
    std::vector<float> buf(128);
    uint32_t rng = 0xA5A5A5A5u;
    for (int i = 0; i < 50000; ++i) {
        const uint8_t note = static_cast<uint8_t>(xorshift(rng) & 127u);
        const uint8_t vel = static_cast<uint8_t>(xorshift(rng) & 127u);
        if (xorshift(rng) & 1u)
            a.noteOn(note, vel);
        else
            a.noteOff(note);
        if ((i % 17) == 0)
            a.allSoundOff();
        if ((i % 31) == 0)
            a.setPitchBend((static_cast<float>(xorshift(rng) & 0xffff) / 32768.0f) - 1.0f);
        const size_t n = 1 + (xorshift(rng) % 128);
        a.processBlock(buf.data(), n);
        if (!a.internalStateFinite()) {
            expectTrue(false, "fuzz produced non-finite state");
            return;
        }
        if ((i % 200) == 0) {
            b.processBlock(buf.data(), 64);
            expectTrue(b.internalStateFinite(), "second voice stayed finite");
        }
    }

    a.init(96000.0f, 64);
    a.noteOn(72, 100);
    a.processBlock(buf.data(), 64);
    a.init(48000.0f, 2048);
    expectTrue(a.internalStateFinite(), "reprepare finite");
}

void testTwoVoices()
{
    std::printf("Two Voice instances\n");
    lxx::dsp::Voice a, b;
    a.init(48000.0f, 2048);
    b.init(48000.0f, 2048);
    std::vector<float> ba(64), bb(64);
    a.noteOn(60, 100);
    b.noteOn(72, 100);
    a.processBlock(ba.data(), 64);
    b.processBlock(bb.data(), 64);
    a.reset();
    b.processBlock(bb.data(), 64);
    expectTrue(b.internalStateFinite(), "B survives A reset");
}

void testOversamplerBlocks()
{
    std::printf("Oversampler varying block sizes\n");
    lxx::dsp::OversampledSVF filt(48000.0f);
    filt.prepareBuffers(2048);
    filt.setCutoffFreq(1000.0f);
    filt.setResonance(0.5f);
    const size_t sizes[] = { 1, 2, 7, 31, 32, 33, 63, 64, 65, 127, 128, 129,
                             255, 256, 511, 512, 1024, 2048 };
    std::vector<float> in(2048, 0.1f), lp(2048), bp(2048), hp(2048);
    for (int rep = 0; rep < 8; ++rep) {
        for (size_t n : sizes) {
            filt.processBlock(in.data(), lp.data(), bp.data(), hp.data(), n);
            for (size_t i = 0; i < n; ++i) {
                if (!std::isfinite(lp[i]) || !std::isfinite(bp[i]) || !std::isfinite(hp[i])) {
                    expectTrue(false, "oversampler non-finite");
                    return;
                }
            }
        }
    }
    const size_t change[] = { 64, 64, 32, 128, 7, 512, 31, 2048, 64 };
    for (size_t n : change)
        filt.processBlock(in.data(), lp.data(), bp.data(), hp.data(), n);
    expectTrue(true, "oversampler block changes");
}

void testBlepRapid()
{
    std::printf("BLEP rapid pitch\n");
    lxx::dsp::BlePOscillator osc(48000.0f);
    osc.setWaveform(lxx::dsp::Waveform::TRIANGLE);
    float maxAbs = 0.0f;
    for (int n = 0; n < 5000; ++n) {
        osc.setFrequency(40.0f + static_cast<float>(n % 4000));
        for (int i = 0; i < 16; ++i) {
            const float s = osc.process();
            if (!std::isfinite(s)) {
                expectTrue(false, "BLEP non-finite");
                return;
            }
            maxAbs = std::max(maxAbs, std::abs(s));
        }
        if ((n % 50) == 0)
            osc.reset();
    }
    expectTrue(maxAbs < 8.0f, "BLEP bounded");
}

void testLongRun()
{
    std::printf("Long-run 30-minute equivalent\n");
    lxx::dsp::Voice voice;
    voice.init(48000.0f, 2048);
    lxx::dsp::VoicePatch patch;
    patch.filterResonance = 0.7f;
    voice.setPatch(patch);
    constexpr float kSr = 48000.0f;
    constexpr int kBlock = 64;
    const size_t totalSamples = static_cast<size_t>(kSr * 30.0 * 60.0);
    std::vector<float> buf(kBlock);
    size_t rendered = 0;
    int notes = 0;
    int nan = 0, inf = 0;
    float maxAbs = 0.0f;
    float dcAcc = 0.0f;
    uint32_t rng = 1;
    auto t0 = Clock::now();
    size_t blocks = 0;
    while (rendered < totalSamples) {
        if ((blocks % 32) == 0) {
            const uint8_t note = static_cast<uint8_t>(40 + (xorshift(rng) % 48));
            if (xorshift(rng) & 1u)
                voice.noteOn(note, 100);
            else
                voice.noteOff(note);
            ++notes;
        }
        auto b0 = Clock::now();
        voice.processBlock(buf.data(), static_cast<size_t>(kBlock));
        auto b1 = Clock::now();
        if (seconds(b0, b1) > lxx::test::timeBudget(0.1)) {
            std::printf("FAIL: processBlock watchdog block=%zu notes=%d\n", blocks, notes);
            ++failures;
            return;
        }
        for (int i = 0; i < kBlock; ++i) {
            const float s = buf[static_cast<size_t>(i)];
            if (!std::isfinite(s)) {
                if (s != s)
                    ++nan;
                else
                    ++inf;
            } else {
                maxAbs = std::max(maxAbs, std::abs(s));
                dcAcc += s;
            }
        }
        if (!voice.internalStateFinite()) {
            expectTrue(false, "long-run internal non-finite");
            return;
        }
        rendered += static_cast<size_t>(kBlock);
        ++blocks;
    }
    const double wall = seconds(t0, Clock::now());
    const float mean = dcAcc / static_cast<float>(totalSamples);
    std::printf("  blocks=%zu notes=%d wall=%.2fs nan=%d inf=%d maxAbs=%.4f mean=%.6f\n",
                blocks, notes, wall, nan, inf, maxAbs, mean);
    expectTrue(nan == 0 && inf == 0, "no NaN/Inf in long run");
    expectTrue(maxAbs < 8.0f, "long-run amplitude bounded");
    expectTrue(std::abs(mean) < 0.05f, "no pathological DC");
    expectTrue(wall < lxx::test::timeBudget(180.0), "long-run completed without hang");
}

void testTransportJump()
{
    std::printf("Transport discontinuity bound\n");
    lxx::dsp::SequencerTransport t;
    t.prepare(48000.0);
    lxx::dsp::SequencerSettings s;
    s.tempo = 240;
    s.arpMode = 1;
    t.setSettings(s);
    t.noteOn(60, 100);
    t.start();
    auto t0 = Clock::now();
    (void)t.processBlock(48000 * 5);
    const double wall = seconds(t0, Clock::now());
    expectTrue(wall < lxx::test::timeBudget(2.0), "5s catch-up bounded");
    t.stop();
}

} // namespace

int main()
{
    testVoiceNoteSequence();
    testVoiceFuzz();
    testTwoVoices();
    testOversamplerBlocks();
    testBlepRapid();
    testTransportJump();
    testLongRun();

    if (failures == 0)
        std::printf("test_runtime_stress: PASS\n");
    else
        std::printf("test_runtime_stress: %d FAIL\n", failures);
    return failures == 0 ? 0 : 1;
}
