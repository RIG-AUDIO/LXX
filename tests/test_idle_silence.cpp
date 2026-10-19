#include "core/dc_blocker.h"
#include "filters/svf_anushri.h"
#include "voice/voice.h"

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

struct IdleStats {
    float peak = 0.0f;
    double rms = 0.0;
    double mean = 0.0;
    size_t nonzero = 0;
    int nanCount = 0;
    int infCount = 0;
};

IdleStats measure(const float* data, size_t n)
{
    IdleStats s;
    double sum = 0.0;
    double sumSq = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const float x = data[i];
        if (!std::isfinite(x)) {
            if (std::isnan(x))
                ++s.nanCount;
            else
                ++s.infCount;
            continue;
        }
        const float ax = std::abs(x);
        s.peak = std::max(s.peak, ax);
        if (x != 0.0f)
            ++s.nonzero;
        sum += static_cast<double>(x);
        sumSq += static_cast<double>(x) * static_cast<double>(x);
    }
    if (n > 0) {
        s.mean = sum / static_cast<double>(n);
        s.rms = std::sqrt(sumSq / static_cast<double>(n));
    }
    return s;
}

void testDefaultIdle()
{
    std::printf("Default idle 10s @ 48 kHz / 64\n");
    lxx::dsp::Voice voice;
    voice.init(48000.0f, 2048);
    std::vector<float> buf(64, 99.0f);
    IdleStats acc;
    size_t samples = 0;
    for (int b = 0; b < 48000 * 10 / 64; ++b) {
        std::fill(buf.begin(), buf.end(), 99.0f);
        voice.processBlock(buf.data(), buf.size());
        const auto s = measure(buf.data(), buf.size());
        acc.peak = std::max(acc.peak, s.peak);
        acc.nonzero += s.nonzero;
        acc.nanCount += s.nanCount;
        acc.infCount += s.infCount;
        acc.mean += s.mean * static_cast<double>(buf.size());
        acc.rms += s.rms * s.rms * static_cast<double>(buf.size());
        samples += buf.size();
        expectTrue(voice.isIdle(), "idle: no notes, envelope dead");
        expectTrue(voice.getAmpSegment() == lxx::dsp::EnvSegment::DEAD, "amp DEAD");
        expectTrue(voice.getAmpEnvelope() == 0.0f, "amp value 0");
        expectTrue(voice.getVcaGain() == 0.0f, "VCA gain 0");
        expectTrue(voice.heldNotes() == 0, "held notes 0");
    }
    acc.mean /= static_cast<double>(samples);
    acc.rms = std::sqrt(acc.rms / static_cast<double>(samples));
    std::printf("  peak=%.8f rms=%.8f dc=%.8f nonzero=%zu nan=%d inf=%d\n",
                acc.peak, acc.rms, acc.mean, acc.nonzero, acc.nanCount, acc.infCount);
    expectTrue(acc.peak == 0.0f && acc.nonzero == 0, "default idle is exact silence");
    expectTrue(acc.nanCount == 0 && acc.infCount == 0, "idle finite");
}

void testPostNoteSilence()
{
    std::printf("Post-note silence\n");
    const float rates[] = { 44100.0f, 48000.0f, 88200.0f, 96000.0f };
    const int blocks[] = { 32, 64, 128, 256, 512 };
    for (float sr : rates) {
        for (int block : blocks) {
            lxx::dsp::Voice voice;
            voice.init(sr, 2048);
            std::vector<float> buf(static_cast<size_t>(block));
            voice.noteOn(60, 100);
            for (int i = 0; i < 32; ++i)
                voice.processBlock(buf.data(), buf.size());
            expectTrue(!voice.isIdle(), "note held is not idle");
            voice.noteOff(60);
            const int wait = static_cast<int>(sr * 2.0f) / block + 8;
            for (int i = 0; i < wait; ++i)
                voice.processBlock(buf.data(), buf.size());
            const auto s = measure(buf.data(), buf.size());
            expectTrue(voice.isIdle(), "returns to idle after release");
            expectTrue(voice.getAmpSegment() == lxx::dsp::EnvSegment::DEAD, "DEAD after release");
            expectTrue(s.peak == 0.0f, "post-note block is silence");
            expectTrue(voice.internalStateFinite(), "post-note finite");
        }
    }
}

void testRepeatedNoteIdle()
{
    std::printf("100k note/release/silence cycles\n");
    lxx::dsp::Voice voice;
    voice.init(48000.0f, 2048);
    lxx::dsp::VoicePatch patch = voice.getPatch();
    patch.envRelease = 0.01f;
    patch.envVcaMorph = 1.0f;
    voice.setPatch(patch);
    std::vector<float> buf(64);
    int fail = 0;
    float residual = 0.0f;
    for (int i = 0; i < 100000; ++i) {
        const uint8_t note = static_cast<uint8_t>(40 + (i % 48));
        voice.noteOn(note, 100);
        voice.processBlock(buf.data(), buf.size());
        voice.noteOff(note);
        for (int w = 0; w < 64 && !voice.isIdle(); ++w)
            voice.processBlock(buf.data(), buf.size());
        if (!voice.isIdle() || voice.heldNotes() != 0
            || voice.getAmpSegment() != lxx::dsp::EnvSegment::DEAD
            || !voice.internalStateFinite()) {
            ++fail;
            break;
        }
        const auto s = measure(buf.data(), buf.size());
        residual = std::max(residual, s.peak);
        if (s.peak != 0.0f) {
            ++fail;
            break;
        }
    }
    std::printf("  failures=%d residual=%.8f\n", fail, residual);
    expectTrue(fail == 0 && residual == 0.0f, "100k cycles return to exact silence");
}

void testAllNotesOff()
{
    lxx::dsp::Voice voice;
    voice.init(48000.0f, 512);
    std::vector<float> buf(64);
    voice.noteOn(60, 100);
    voice.noteOn(64, 100);
    voice.processBlock(buf.data(), buf.size());
    voice.allSoundOff();
    voice.processBlock(buf.data(), buf.size());
    expectTrue(voice.isIdle() && voice.heldNotes() == 0, "allSoundOff idles immediately");
    expectTrue(measure(buf.data(), buf.size()).peak == 0.0f, "allSoundOff silence");
}

void testDcBlockerZero()
{
    lxx::dsp::DcBlocker dc;
    dc.prepare(48000.0);
    float peak = 0.0f;
    for (int i = 0; i < 5'000'000; ++i) {
        const float y = dc.process(0.0f);
        peak = std::max(peak, std::abs(y));
    }
    std::printf("DC blocker zero-input peak=%.8e\n", peak);
    expectTrue(peak == 0.0f, "DC blocker zeros stay zero");
}

void testFilterIdleNotSelfOsc()
{
    lxx::dsp::SVFAnushriLinear svf(96000.0f);
    svf.setCutoffFreq(1000.0f);
    svf.setResonance(0.2f);
    float peak = 0.0f;
    for (int i = 0; i < 48000; ++i) {
        const auto o = svf.process(0.0f);
        peak = std::max(peak, std::abs(o[0]));
    }
    expectTrue(peak < 1.0e-5f, "default resonance does not self-oscillate on zero input");

    lxx::dsp::Voice voice;
    voice.init(48000.0f, 2048);
    lxx::dsp::VoicePatch p = voice.getPatch();
    p.filterResonance = 0.2f;
    voice.setPatch(p);
    std::vector<float> buf(64);
    for (int i = 0; i < 1000; ++i)
        voice.processBlock(buf.data(), buf.size());
    expectTrue(measure(buf.data(), buf.size()).peak == 0.0f, "default-res voice idle silent");
}

void testMultiVoiceIdle()
{
    lxx::dsp::Voice a, b;
    a.init(48000.0f, 512);
    b.init(48000.0f, 512);
    std::vector<float> ba(64), bb(64);
    a.noteOn(60, 100);
    a.processBlock(ba.data(), ba.size());
    b.processBlock(bb.data(), bb.size());
    expectTrue(measure(bb.data(), bb.size()).peak == 0.0f, "idle voice B silent while A plays");
    a.noteOff(60);
    for (int i = 0; i < 256; ++i) {
        a.processBlock(ba.data(), ba.size());
        b.processBlock(bb.data(), bb.size());
    }
    expectTrue(a.isIdle() && b.isIdle(), "both idle after A release");
}

} // namespace

int main()
{
    testDefaultIdle();
    testPostNoteSilence();
    testRepeatedNoteIdle();
    testAllNotesOff();
    testDcBlockerZero();
    testFilterIdleNotSelfOsc();
    testMultiVoiceIdle();

    if (failures == 0)
        std::printf("test_idle_silence: PASS\n");
    else
        std::printf("test_idle_silence: %d FAIL\n", failures);
    return failures == 0 ? 0 : 1;
}
