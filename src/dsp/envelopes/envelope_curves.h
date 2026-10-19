#pragma once

#include "../core/dsp_types.h"
#include <array>

namespace lxx {
namespace dsp {

/**
 * Envelope curve generation
 * 
 * Must numerically match lookup_tables.py: env_expo
 * gamma = 0.175, max_time = 12s, min_time = 3/control_rate
 * 
 * The curve is: env_expo = 1 - exp(-4 * t) normalized to [0, 65535]
 * But the lookup table uses a gamma-law mapping for time values.
 */

// Envelope curve types
enum class EnvCurveType {
    Linear,
    Exponential,      // Original Anushri: 1 - exp(-4*t)
    ExponentialFast,  // Steeper
    Quadratic,        // 1 - (1-t)^2
};

// Pre-computed envelope curve lookup table
// 257 points (0-256 inclusive) matching lut_res_env_expo
class EnvelopeCurves {
public:
    static constexpr size_t kTableSize = 257;
    
    EnvelopeCurves();
    
    // Get curve value at normalized phase [0, 1]
    Sample getValue(EnvCurveType type, Sample phase) const;
    
    // Get curve value at 16-bit phase (0-65535)
    Sample getValue16(EnvCurveType type, uint16_t phase) const;
    
    // Get curve value at 32-bit phase (for high-res envelopes)
    Sample getValue32(EnvCurveType type, uint32_t phase) const;
    
    // Firmware control rates as evaluated by upstream anu/resources/lookup_tables.py.
    //
    // The script is Python 2, so `20000000 / 510` is integer division and
    // yields 39215 *before* the float divisions that follow.  The published
    // firmware tables (anu/resources.cc) were generated with those values:
    //   voice tables: 39215 / 8.0 / 2.0 = 2450.9375 Hz
    //   drum tables:  39215 / 32.0      = 1225.46875 Hz
    // The real hardware control rate is 20000000/510/8/2 = 2450.98039 Hz; the
    // 1.7e-5 relative difference is part of the firmware tables, so table
    // generation reproduces it exactly.  tests/test_firmware_tables.cpp
    // asserts bit-exact reproduction of all four published tables.
    static constexpr double kVoiceControlRate = (20000000 / 510) / 8.0 / 2.0;
    static constexpr double kDrumControlRate  = (20000000 / 510) / 32.0;

    // The generators take double arguments and truncate like numpy's
    // `astype(int)`: the upstream tables are generated in double precision and
    // are asserted against the published firmware tables in
    // tests/test_firmware_tables.cpp.

    // Generate gamma-law time mapping (matches lookup_tables.py)
    // gamma = 0.175, 256 values, 32.32 increments
    static std::array<uint32_t, 256> generateEnvIncrements(
        double maxTime = 12.0,
        double minTime = 3.0 / kVoiceControlRate,
        double gamma = 0.175,
        double controlRate = kVoiceControlRate
    );

    // Generate drum envelope increments (gamma = 0.25, min_time = 4/control)
    static std::array<uint16_t, 256> generateDrumEnvIncrements(
        double maxTime = 4.0,
        double gamma = 0.25,
        double controlRate = kDrumControlRate
    );

    // Generate glide increments (gamma = 0.1, min_time = 3/control)
    static std::array<uint16_t, 256> generateGlideIncrements(
        double maxTime = 6.0,
        double gamma = 0.1,
        double controlRate = kVoiceControlRate
    );

    // Generate LFO increments (32-bit, linear in log space)
    static std::array<uint32_t, 256> generateLfoIncrements(
        double minFreq = 1.0 / 16.0,
        double maxFreq = 100.0,
        double controlRate = kVoiceControlRate
    );

    // Access to raw tables for validation
    const std::array<Sample, kTableSize>& getTable(EnvCurveType type) const;

private:
    std::array<Sample, kTableSize> linearTable_;
    std::array<Sample, kTableSize> expoTable_;
    std::array<Sample, kTableSize> expoFastTable_;
    std::array<Sample, kTableSize> quadraticTable_;
    
    void generateTables();
};

// Inline exponential curve (matches lookup_tables.py exactly)
// env_expo = 1.0 - exp(-4.0 * t) normalized
inline Sample envExpoCurve(Sample t) {
    t = clamp(t, 0.0f, 1.0f);
    Sample raw = 1.0f - std::exp(-4.0f * t);
    // Normalize so max = 1.0 (precomputed: 1 - exp(-4) = 0.98168436)
    constexpr Sample maxVal = 0.98168436f;
    return raw / maxVal;
}

// Fast approximation for real-time
inline Sample envExpoCurveFast(Sample t) {
    t = clamp(t, 0.0f, 1.0f);
    // Polynomial approximation of 1 - exp(-4t) / (1 - exp(-4))
    // Using minimax approximation
    return t * (1.0f + t * (0.5f + t * (0.166667f + t * 0.0416667f)));
}

} // namespace dsp
} // namespace lxx
