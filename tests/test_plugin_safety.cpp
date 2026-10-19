#include "core/dc_blocker.h"
#include "oscillators/blep_oscillator.h"
#include "voice/voice.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

int failures = 0;

void expectTrue(bool condition, const char* message)
{
    if (!condition) {
        std::printf("FAIL: %s\n", message);
        ++failures;
    }
}

struct Stats {
    float peak = 0.0f;
    double mean = 0.0;
    int nanCount = 0;
    int infCount = 0;
};

Stats measure(const float* data, size_t n)
{
    Stats s;
    double sum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const float x = data[i];
        if (!std::isfinite(x)) {
            if (std::isnan(x))
                ++s.nanCount;
            else
                ++s.infCount;
            continue;
        }
        s.peak = std::max(s.peak, std::abs(x));
        sum += static_cast<double>(x);
    }
    if (n > 0)
        s.mean = sum / static_cast<double>(n);
    return s;
}

void testBlepInstanceIsolation()
{
    lxx::dsp::BlePOscillator a(48000.0f);
    lxx::dsp::BlePOscillator b(48000.0f);
    a.setWaveform(lxx::dsp::Waveform::TRIANGLE);
    b.setWaveform(lxx::dsp::Waveform::TRIANGLE);
    a.setFrequency(220.0f);
    b.setFrequency(440.0f);
    a.reset();
    b.reset();

    std::vector<float> firstA(64);
    for (auto& s : firstA)
        s = a.process();

    for (int i = 0; i < 1000; ++i)
        b.process();

    a.reset();
    a.setFrequency(220.0f);
    bool match = true;
    for (float expected : firstA) {
        const float got = a.process();
        if (std::abs(got - expected) > 1.0e-6f)
            match = false;
    }
    expectTrue(match, "triangle integrator state is per-oscillator, not static");
}

void testVoiceBlockSizes()
{
    const int blocks[] = { 1, 2, 7, 16, 31, 32, 63, 64, 65, 127, 128, 255, 256, 511, 512, 1024, 2048 };
    lxx::dsp::Voice voice;
    voice.init(48000.0f, 2048);
    voice.noteOn(60, 100);

    for (int block : blocks) {
        std::vector<float> buf(static_cast<size_t>(block), 99.0f);
        voice.processBlock(buf.data(), static_cast<size_t>(block));
        const auto s = measure(buf.data(), buf.size());
        expectTrue(s.nanCount == 0, "block size nan");
        expectTrue(s.infCount == 0, "block size inf");
        char msg[64];
        std::snprintf(msg, sizeof(msg), "block %d wrote output", block);
        expectTrue(s.peak < 8.0f, msg);
    }

    std::vector<float> empty(8, 1.0f);
    voice.processBlock(empty.data(), 0);
    expectTrue(empty[0] == 1.0f, "zero-length block does not write");
}

void testVoiceSampleRates()
{
    const float rates[] = { 44100.0f, 48000.0f, 88200.0f, 96000.0f };
    for (float rate : rates) {
        lxx::dsp::Voice voice;
        voice.init(rate, 512);
        voice.noteOn(69, 100);
        std::vector<float> buf(512, 0.0f);
        voice.processBlock(buf.data(), buf.size());
        const auto s = measure(buf.data(), buf.size());
        expectTrue(s.nanCount == 0 && s.infCount == 0, "sample-rate finite");
        expectTrue(s.peak > 0.0f, "note produces signal");
    }
}

void testVoiceUnpreparedSilence()
{
    lxx::dsp::Voice voice;
    std::vector<float> buf(128, 1.0f);
    voice.processBlock(buf.data(), buf.size());
    bool silent = true;
    for (float x : buf) {
        if (x != 0.0f)
            silent = false;
    }
    expectTrue(silent, "uninitialized voice renders silence");
}

void testTwoVoicesIndependent()
{
    lxx::dsp::Voice a;
    lxx::dsp::Voice b;
    a.init(48000.0f, 256);
    b.init(48000.0f, 256);
    a.noteOn(48, 110);
    b.noteOn(84, 110);
    std::vector<float> bufA(256), bufB(256);
    a.processBlock(bufA.data(), bufA.size());
    b.processBlock(bufB.data(), bufB.size());
    expectTrue(measure(bufA.data(), bufA.size()).nanCount == 0, "voice A finite");
    expectTrue(measure(bufB.data(), bufB.size()).nanCount == 0, "voice B finite");
    a.allSoundOff();
    b.processBlock(bufB.data(), bufB.size());
    expectTrue(measure(bufB.data(), bufB.size()).nanCount == 0, "voice B survives A destroy path");
}

void testDcBlocker()
{
    lxx::dsp::DcBlocker dc;
    dc.prepare(48000.0);
    float y = 0.0f;
    for (int i = 0; i < 48000; ++i)
        y = dc.process(1.0f);
    expectTrue(std::isfinite(y), "dc blocker finite");
    expectTrue(std::abs(y) < 0.05f, "dc blocker settles on constant");
}

void testVoiceLifecycle()
{
    for (int i = 0; i < 200; ++i) {
        lxx::dsp::Voice voice;
        const float rate = (i % 2 == 0) ? 48000.0f : 44100.0f;
        voice.init(rate, static_cast<size_t>(64 + (i % 8) * 32));
        voice.noteOn(static_cast<uint8_t>(36 + (i % 48)), 100);
        std::vector<float> buf(64, 0.0f);
        voice.processBlock(buf.data(), buf.size());
        const auto s = measure(buf.data(), buf.size());
        expectTrue(s.nanCount == 0 && s.infCount == 0, "lifecycle finite");
        voice.init(96000.0f, 128);
        voice.processBlock(buf.data(), buf.size());
    }
}

void testStressFinite()
{
    lxx::dsp::Voice voice;
    voice.init(48000.0f, 1024);
    lxx::dsp::VoicePatch patch = voice.getPatch();
    patch.filterResonance = 0.99f;
    patch.filterCutoffHz = 80.0f;
    patch.waveLevel = 1.0f;
    patch.subLevel = 1.0f;
    patch.volume = 1.0f;
    patch.unisonCount = 4;
    voice.setPatch(patch);

    Stats global;
    double absSum = 0.0;
    size_t total = 0;

    for (int n = 0; n < 40; ++n) {
        voice.noteOn(static_cast<uint8_t>(24 + (n % 48)), 127);
        std::vector<float> buf(256);
        voice.processBlock(buf.data(), buf.size());
        const auto s = measure(buf.data(), buf.size());
        expectTrue(s.nanCount == 0 && s.infCount == 0, "stress finite");
        global.peak = std::max(global.peak, s.peak);
        global.nanCount += s.nanCount;
        global.infCount += s.infCount;
        absSum += std::abs(s.mean) * static_cast<double>(buf.size());
        total += buf.size();
        if (n % 3 == 0)
            voice.noteOff(static_cast<uint8_t>(24 + (n % 48)));
    }

    lxx::dsp::DcBlocker dc;
    dc.prepare(48000.0);
    voice.noteOn(60, 127);
    std::vector<float> longBuf(48000);
    voice.processBlock(longBuf.data(), longBuf.size());
    for (float& x : longBuf)
        x = dc.process(x);
    const auto longStats = measure(longBuf.data(), longBuf.size());
    global.peak = std::max(global.peak, longStats.peak);
    global.nanCount += longStats.nanCount;
    global.infCount += longStats.infCount;

    std::printf("output_safety peak=%.6f dc_mean=%.6e nan=%d inf=%d long_peak=%.6f long_dc=%.6e\n",
                global.peak,
                total > 0 ? absSum / static_cast<double>(total) : 0.0,
                global.nanCount,
                global.infCount,
                longStats.peak,
                longStats.mean);
}

}  // namespace

int main()
{
    testBlepInstanceIsolation();
    testVoiceBlockSizes();
    testVoiceSampleRates();
    testVoiceUnpreparedSilence();
    testTwoVoicesIndependent();
    testDcBlocker();
    testVoiceLifecycle();
    testStressFinite();

    if (failures == 0) {
        std::printf("test_plugin_safety: PASS\n");
        return 0;
    }
    std::printf("test_plugin_safety: %d FAIL\n", failures);
    return 1;
}
