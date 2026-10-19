// Glide (portamento) duration fidelity.
//
// Firmware reference: anu/voice.cc @ upstream e030d7b
//   pitch_counter_ (uint16) += pitch_increment_ once per control period;
//   on wrap the sweep is complete.  The increment comes from
//   lut_res_glide_increments[slide_time], so a full sweep takes
//
//       duration = 65536 / pitch_increment_  control periods
//                = 65536 / (increment * control_rate)  seconds
//
// LXX keeps the same counter in 16.16 form (uint32, balance = counter >> 16)
// so the sweep advances per audio sample.  This test measures the real sample
// count until the sweep completes and compares it with the firmware law, so a
// missing or extra 65536 factor in the scaling shows up immediately.
//
// Note on the control mapping: LXX's glideTimeSec parameter maps linearly onto
// the firmware slide_time index 0..255 (the hardware panel domain).  The
// firmware table itself is a gamma law, so index -> time is deliberately
// non-linear; that non-linearity is firmware behaviour, not an error.
#include "../src/dsp/envelopes/envelope_curves.h"
#include "../src/dsp/voice/glide.h"
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace {

int failures = 0;

// Rate applied by the AVR clock (20 MHz / 510 cycles / 8 / 2).
constexpr double kHardwareControlRate = 20000000.0 / 510.0 / 8.0 / 2.0;

// Index selection used by PitchGlide::setGlideTime().
int indexFor(double timeSec)
{
    const double t = timeSec < 0.0 ? 0.0 : (timeSec > 6.0 ? 6.0 : timeSec);
    const int index = static_cast<int>(std::round((t / 6.0) * 255.0));
    return index < 0 ? 0 : (index > 255 ? 255 : index);
}

// Samples until the 16.16 counter wraps, given the firmware increment.
uint64_t expectedSamples(uint16_t inc, double sampleRate)
{
    const double perSample = static_cast<double>(inc)
                           * (kHardwareControlRate / sampleRate) * 65536.0;
    const auto q = static_cast<uint32_t>(perSample);
    if (q == 0) return 0;
    return static_cast<uint64_t>(std::ceil(4294967296.0 / static_cast<double>(q)));
}

// Runs one glide and returns the number of process() calls until the target
// pitch is reached, or -1 if the glide was instant.
int64_t measure(double timeSec, double sampleRate, int16_t target)
{
    lxx::dsp::PitchGlide glide;
    glide.init(static_cast<lxx::dsp::Sample>(sampleRate));
    glide.snapTo(0);
    glide.setGlideTime(static_cast<lxx::dsp::Sample>(timeSec));
    glide.glideTo(target);
    if (glide.isInstant()) return -1;

    const int64_t limit = static_cast<int64_t>(sampleRate * 120.0);
    int64_t n = 0;
    while (glide.currentPitch() != target) {
        glide.process();
        ++n;
        if (n > limit) return -2;
    }
    return n;
}

void checkCase(double timeSec, double sampleRate)
{
    const auto table = lxx::dsp::EnvelopeCurves::generateGlideIncrements();
    const int index = indexFor(timeSec);
    const uint16_t inc = table[static_cast<size_t>(index)];
    const uint64_t want = expectedSamples(inc, sampleRate);
    const int64_t got = measure(timeSec, sampleRate, 7680);

    if (want == 0) {
        std::printf("FAIL: t=%.4f sr=%.0f idx=%d increment is 0\n",
                    timeSec, sampleRate, index);
        ++failures;
        return;
    }
    if (got < 0) {
        std::printf("FAIL: t=%.4f sr=%.0f idx=%d measure=%lld (instant or hung)\n",
                    timeSec, sampleRate, index, static_cast<long long>(got));
        ++failures;
        return;
    }

    const double measured = static_cast<double>(got);
    const double expected = static_cast<double>(want);
    const double err = std::abs(measured - expected) / expected;
    const double errMs = std::abs(measured - expected) * 1.0e3 / sampleRate;
    if (err > 0.002 && errMs > 1.0) {
        std::printf("FAIL: t=%.4f sr=%.0f idx=%d inc=%u got=%lld want=%llu "
                    "(%.3f%%)\n",
                    timeSec, sampleRate, index, inc,
                    static_cast<long long>(got),
                    static_cast<unsigned long long>(want), err * 100.0);
        ++failures;
    } else {
        std::printf("  t=%7.4fs sr=%5.0f idx=%3d inc=%5u samples=%llu "
                    "(expected %llu, %.4f%%)\n",
                    timeSec, sampleRate, index, inc,
                    static_cast<unsigned long long>(got),
                    static_cast<unsigned long long>(want), err * 100.0);
    }
}

void checkFirmwareMinimum(double sampleRate)
{
    // slide_time 0 is the fastest firmware glide: min_time = 3 control periods.
    const auto table = lxx::dsp::EnvelopeCurves::generateGlideIncrements();
    const double wantSec = 65536.0 / (static_cast<double>(table[0])
                                      * kHardwareControlRate);
    const double nominalSec = 3.0 / kHardwareControlRate;
    const int64_t got = measure(nominalSec, sampleRate, 7680);
    const double gotSec = static_cast<double>(got) / sampleRate;
    const double err = std::abs(gotSec - wantSec) / wantSec;
    std::printf("  fastest glide: measured %.6fs firmware %.6fs (%.4f%%)\n",
                gotSec, wantSec, err * 100.0);
    if (got <= 0 || err > 0.002) {
        std::printf("FAIL: fastest firmware glide duration\n");
        ++failures;
    }
    if (std::abs(nominalSec - wantSec) / wantSec > 0.02) {
        std::printf("FAIL: min_time 3/control_rate does not match table[0]\n");
        ++failures;
    }
}

void checkInstant(double sampleRate)
{
    const int64_t n = measure(0.0, sampleRate, 7680);
    if (n != -1) {
        std::printf("FAIL: t=0 should be an instant snap, got %lld\n",
                    static_cast<long long>(n));
        ++failures;
    }
}

} // namespace

int main()
{
    std::printf("Glide duration fidelity (firmware law)\n");

    const double rates[] = { 44100.0, 48000.0, 96000.0 };
    const double times[] = { 0.001224, 0.05, 0.25, 1.0, 1.5, 3.0, 5.0, 6.0 };

    for (double sr : rates) {
        std::printf("sample rate %.0f\n", sr);
        checkInstant(sr);
        checkFirmwareMinimum(sr);
        for (double t : times) checkCase(t, sr);
    }

    if (failures == 0)
        std::printf("test_glide: PASS\n");
    else
        std::printf("test_glide: %d FAIL\n", failures);
    return failures == 0 ? 0 : 1;
}
