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
    
    // Generate gamma-law time mapping (matches lookup_tables.py)
    // gamma = 0.175, 256 values
    static std::array<uint32_t, 256> generateEnvIncrements(
        Sample maxTime = kEnvMaxTime,
        Sample minTime = kEnvMinTime,
        Sample gamma = kEnvGamma,
        Sample controlRate = 20000000.0f / 510.0f / 8.0f / 2.0f
    );
    
    // Generate drum envelope increments (gamma = 0.25)
    static std::array<uint16_t, 256> generateDrumEnvIncrements(
        Sample maxTime = kDrmEnvMaxTime,
        Sample gamma = kDrmEnvGamma,
        Sample controlRate = 20000000.0f / 510.0f / 32.0f
    );
    
    // Generate glide increments (gamma = 0.1)
    static std::array<uint16_t, 256> generateGlideIncrements(
        Sample maxTime = kGlideMaxTime,
        Sample gamma = kGlideGamma,
        Sample controlRate = 20000000.0f / 510.0f / 8.0f / 2.0f
    );
    
    // Generate LFO increments (32-bit, linear in log space)
    static std::array<uint32_t, 256> generateLfoIncrements(
        Sample minFreq = kLfoMinFreq,
        Sample maxFreq = kLfoMaxFreq,
        Sample controlRate = 20000000.0f / 510.0f / 8.0f / 2.0f
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