// libFuzzer harness for the lxx_dsp core.
//
// Feeds untrusted bytes into the filter, envelope, drum synth and pattern
// generator, then asserts that every rendered sample is finite.  A non-finite
// sample means a control path escaped the sanitisation the core promises, so
// the harness aborts and the fuzzer reports it as a crash.
//
// Build with -DLXX_BUILD_FUZZ=ON (Clang only):
//   cmake -S . -B build-fuzz -G Ninja \
//     -DCMAKE_BUILD_TYPE=RelWithDebInfo \
//     -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
//     -DLXX_BUILD_FUZZ=ON -DLXX_BUILD_TESTS=OFF -DLXX_BUILD_PLUGIN=OFF
//   cmake --build build-fuzz --target lxx_dsp_fuzz
//   ./build-fuzz/tests/fuzz/lxx_dsp_fuzz -max_len=512 -runs=200000

#include "core/dsp_types.h"
#include "drums/drum_synth.h"
#include "envelopes/adsr_envelope.h"
#include "filters/svf_anushri.h"
#include "sequencer/grids/grids_pattern_generator.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr size_t kFrames = 64;

using lxx::dsp::Sample;

uint32_t le32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16)
         | (static_cast<uint32_t>(p[3]) << 24);
}

float leFloat(const uint8_t* p) {
    const uint32_t bits = le32(p);
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

bool allFinite(const Sample* buf, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (!std::isfinite(buf[i])) return false;
    }
    return true;
}

[[noreturn]] void fail(const char* what) {
    std::fprintf(stderr, "lxx_dsp fuzz: non-finite sample in %s\n", what);
    std::fflush(stderr);
    std::abort();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 64) return 0;

    Sample in[kFrames];
    Sample out[kFrames];
    Sample lp[kFrames];
    Sample bp[kFrames];
    Sample hp[kFrames];

    for (size_t i = 0; i < kFrames; ++i) {
        const int byte = data[i % size];
        in[i] = static_cast<Sample>(byte) / 128.0f - 1.0f;
    }

    // --- state variable filter --------------------------------------------
    {
        lxx::dsp::SVFAnushriLinear filter(lxx::dsp::kSampleRate);
        filter.reset();
        filter.setFilterMode(static_cast<lxx::dsp::SVFOutput>(data[0] % 3));
        filter.setAuthenticMode((data[1] & 1u) != 0);
        filter.setUseAnuResonance((data[2] & 1u) != 0);

        // Both well-formed and hostile control values.
        filter.setCutoffFreq(20.0f + 400.0f * static_cast<Sample>(data[3]));
        filter.setResonance(static_cast<Sample>(data[4]) / 255.0f);
        filter.setCutoffCV(leFloat(data + 5), leFloat(data + 9), leFloat(data + 13));
        filter.setResonanceCV(leFloat(data + 17));

        filter.processBlock(in, lp, bp, hp, kFrames);
        if (!allFinite(lp, kFrames) || !allFinite(bp, kFrames) ||
            !allFinite(hp, kFrames)) {
            fail("SVFAnushriLinear::processBlock");
        }

        // Sample-at-a-time path, with state injected from the input.
        const Sample seedState[3] = {leFloat(data + 21), leFloat(data + 25),
                                     leFloat(data + 29)};
        filter.setInitialState(seedState);
        filter.setCutoffFreq(440.0f);
        for (size_t i = 0; i < kFrames; ++i) {
            const Sample y = filter.processSelected(in[i]);
            if (!std::isfinite(y)) fail("SVFAnushriLinear::processSelected");
        }
    }

    // --- ADSR envelope -----------------------------------------------------
    {
        lxx::dsp::AdsrEnvelope env(lxx::dsp::kSampleRate);
        env.init(lxx::dsp::kSampleRate);
        env.setFromParams(data[33], data[34], data[35], data[36]);
        env.noteOn();
        env.processBlock(out, kFrames);
        env.noteOff();
        env.processBlock(out, kFrames);
        env.retrigger();
        env.processBlock(out, kFrames);
        env.kill();
        env.processBlock(out, kFrames);
        if (!allFinite(out, kFrames)) fail("AdsrEnvelope::processBlock");
    }

    // --- drum synth --------------------------------------------------------
    {
        lxx::dsp::DrumSynth drums;
        drums.init(lxx::dsp::kSampleRate);
        drums.seedRandom(static_cast<uint16_t>(le32(data + 37) & 0xffffu));

        for (uint8_t cc = 0; cc < 24; ++cc) {
            drums.setParameterCc(cc, data[41 + (cc % 20)]);
        }
        drums.setBalance(data[61]);
        drums.setBandwidth(data[62]);
        drums.setVoiceGains(leFloat(data + 45), leFloat(data + 49),
                            leFloat(data + 53));

        const uint8_t instrument = static_cast<uint8_t>(data[0] % 3);
        drums.setEnginePatch(instrument, data[1], data[2], data[3], data[4],
                             data[5]);
        drums.morphPatch(instrument, data[6]);
        drums.trigger(instrument, data[7]);
        drums.processBlock(out, kFrames);
        if (!allFinite(out, kFrames)) fail("DrumSynth::processBlock");

        drums.fillWithSilence(kFrames);
        drums.processBlock(out, kFrames);
        if (!allFinite(out, kFrames)) fail("DrumSynth::processBlock (silence)");
    }

    // --- grids pattern generator ------------------------------------------
    {
        lxx::dsp::grids::PatternGenerator pg;
        pg.reset();
        pg.setSeed(le32(data + 21));

        lxx::dsp::grids::GridsSettings settings;
        settings.x = data[8];
        settings.y = data[9];
        settings.randomness = data[10];
        settings.density[0] = data[11];
        settings.density[1] = data[12];
        settings.density[2] = data[13];
        pg.setSettings(settings);

        for (int i = 0; i < 64; ++i) {
            const auto step = pg.evaluateStep();
            const uint8_t bits = static_cast<uint8_t>(step.triggers & 0x07u);
            if (bits != step.triggers) fail("PatternGenerator::evaluateStep");
            if (step.step >= lxx::dsp::grids::kStepsPerPattern)
                fail("PatternGenerator::evaluateStep step");
        }
    }

    return 0;
}
