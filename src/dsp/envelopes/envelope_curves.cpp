#include "envelope_curves.h"
#include <cmath>

namespace lxx {
namespace dsp {

EnvelopeCurves::EnvelopeCurves() {
    generateTables();
}

void EnvelopeCurves::generateTables() {
    // Linear: y = x
    for (size_t i = 0; i < kTableSize; ++i) {
        Sample t = static_cast<Sample>(i) / static_cast<Sample>(kTableSize - 1);
        linearTable_[i] = t;
    }
    
    // Exponential: 1 - exp(-4*t) normalized (matches lookup_tables.py exactly)
    const Sample maxVal = 1.0f - std::exp(-4.0f);
    for (size_t i = 0; i < kTableSize; ++i) {
        Sample t = static_cast<Sample>(i) / static_cast<Sample>(kTableSize - 1);
        Sample raw = 1.0f - std::exp(-4.0f * t);
        expoTable_[i] = raw / maxVal;
    }
    
    // Exponential fast: 1 - exp(-6*t) normalized
    const Sample maxValFast = 1.0f - std::exp(-6.0f);
    for (size_t i = 0; i < kTableSize; ++i) {
        Sample t = static_cast<Sample>(i) / static_cast<Sample>(kTableSize - 1);
        Sample raw = 1.0f - std::exp(-6.0f * t);
        expoFastTable_[i] = raw / maxValFast;
    }
    
    // Quadratic: 1 - (1-t)^2
    for (size_t i = 0; i < kTableSize; ++i) {
        Sample t = static_cast<Sample>(i) / static_cast<Sample>(kTableSize - 1);
        quadraticTable_[i] = 1.0f - (1.0f - t) * (1.0f - t);
    }
}

Sample EnvelopeCurves::getValue(EnvCurveType type, Sample phase) const {
    phase = clamp(phase, 0.0f, 1.0f);
    Sample index = phase * static_cast<Sample>(kTableSize - 1);
    size_t i = static_cast<size_t>(index);
    Sample frac = index - static_cast<Sample>(i);
    
    const auto& table = getTable(type);
    if (i >= kTableSize - 1) return table[kTableSize - 1];
    
    return lerp(table[i], table[i + 1], frac);
}

Sample EnvelopeCurves::getValue16(EnvCurveType type, uint16_t phase) const {
    // phase is 0-65535
    Sample normalized = phase * (1.0f / 65535.0f);
    return getValue(type, normalized);
}

Sample EnvelopeCurves::getValue32(EnvCurveType type, uint32_t phase) const {
    // phase is 0-4294967295, use upper 16 bits
    uint16_t phase16 = static_cast<uint16_t>(phase >> 16);
    return getValue16(type, phase16);
}

const std::array<Sample, EnvelopeCurves::kTableSize>& EnvelopeCurves::getTable(EnvCurveType type) const {
    switch (type) {
        case EnvCurveType::Linear: return linearTable_;
        case EnvCurveType::Exponential: return expoTable_;
        case EnvCurveType::ExponentialFast: return expoFastTable_;
        case EnvCurveType::Quadratic: return quadraticTable_;
    }
    return expoTable_;
}

// Gamma-law time increment generation (matches lookup_tables.py exactly)
//
// The upstream script computes these tables in double precision with
// `numpy.linspace(...)` and stores them with `astype(int)` (truncation).  Both
// properties are reproduced here: double arithmetic, then a truncating cast.
//
//   max_time, min_time are expressed in seconds, so the increments are
//   65536/(time * control_rate) for the 16-bit tables and
//   (65536*65536)/(time * control_rate) for the 32-bit table.
std::array<uint32_t, 256> EnvelopeCurves::generateEnvIncrements(
    double maxTime, double minTime, double gamma, double controlRate) {

    std::array<uint32_t, 256> values;

    const double scale = 65536.0 * 65536.0;
    const double minIncrement = scale / (maxTime * controlRate);
    const double maxIncrement = scale / (minTime * controlRate);

    // From lookup_tables.py:
    // rates = linspace(max_increment^(-gamma), min_increment^(-gamma), 256)
    // values = rates^(-1/gamma)

    const double start = std::pow(maxIncrement, -gamma);
    const double end = std::pow(minIncrement, -gamma);

    for (size_t i = 0; i < 256; ++i) {
        const double t = static_cast<double>(i) / 255.0;
        const double rate = start + t * (end - start);
        const double val = std::pow(rate, -1.0 / gamma);
        values[i] = static_cast<uint32_t>(val);
    }

    // First two entries are 0 (from lookup_tables.py: lfo_increments[0:2] = 0)
    // But for envelopes, we keep them as calculated

    return values;
}

std::array<uint16_t, 256> EnvelopeCurves::generateDrumEnvIncrements(
    double maxTime, double gamma, double controlRate) {

    std::array<uint16_t, 256> values;

    // min_time = 4/control_rate  (lookup_tables.py, drum envelope section)
    const double minTime = 4.0 / controlRate;
    const double minIncrement = 65536.0 / (maxTime * controlRate);
    const double maxIncrement = 65536.0 / (minTime * controlRate);

    const double start = std::pow(maxIncrement, -gamma);
    const double end = std::pow(minIncrement, -gamma);

    for (size_t i = 0; i < 256; ++i) {
        const double t = static_cast<double>(i) / 255.0;
        const double rate = start + t * (end - start);
        const double val = std::pow(rate, -1.0 / gamma);
        values[i] = static_cast<uint16_t>(val);
    }

    return values;
}

std::array<uint16_t, 256> EnvelopeCurves::generateGlideIncrements(
    double maxTime, double gamma, double controlRate) {

    std::array<uint16_t, 256> values;

    // min_time = 3/control_rate  (lookup_tables.py, glide table section)
    const double minTime = 3.0 / controlRate;
    const double minIncrement = 65536.0 / (maxTime * controlRate);
    const double maxIncrement = 65536.0 / (minTime * controlRate);

    const double start = std::pow(maxIncrement, -gamma);
    const double end = std::pow(minIncrement, -gamma);

    for (size_t i = 0; i < 256; ++i) {
        const double t = static_cast<double>(i) / 255.0;
        const double rate = start + t * (end - start);
        const double val = std::pow(rate, -1.0 / gamma);
        values[i] = static_cast<uint16_t>(val);
    }

    return values;
}

std::array<uint32_t, 256> EnvelopeCurves::generateLfoIncrements(
    double minFreq, double maxFreq, double controlRate) {

    std::array<uint32_t, 256> values;

    const double minIncrement = 65536.0 * 65536.0 * minFreq / controlRate;
    const double maxIncrement = 65536.0 * 65536.0 * maxFreq / controlRate;

    // Linear in log space
    const double logMin = std::log(minIncrement);
    const double logMax = std::log(maxIncrement);

    for (size_t i = 0; i < 256; ++i) {
        const double t = static_cast<double>(i) / 255.0;
        const double logVal = logMin + t * (logMax - logMin);
        const double val = std::exp(logVal);
        values[i] = static_cast<uint32_t>(val);
    }

    // First two entries = 0 (from lookup_tables.py)
    values[0] = 0;
    values[1] = 0;

    return values;
}

} // namespace dsp
} // namespace lxx
