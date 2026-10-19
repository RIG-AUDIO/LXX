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
std::array<uint32_t, 256> EnvelopeCurves::generateEnvIncrements(
    Sample maxTime, Sample minTime, Sample gamma, Sample controlRate) {
    
    std::array<uint32_t, 256> values;
    
    Sample minIncrement = 65536.0f * 65536.0f / (maxTime * controlRate);
    Sample maxIncrement = 65536.0f * 65536.0f / (minTime * controlRate);
    
    // From lookup_tables.py:
    // rates = linspace(max_increment^(-gamma), min_increment^(-gamma), 256)
    // values = rates^(-1/gamma)
    
    Sample start = std::pow(maxIncrement, -gamma);
    Sample end = std::pow(minIncrement, -gamma);
    
    for (size_t i = 0; i < 256; ++i) {
        Sample t = static_cast<Sample>(i) / 255.0f;
        Sample rate = start + t * (end - start);
        Sample val = std::pow(rate, -1.0f / gamma);
        values[i] = static_cast<uint32_t>(val + 0.5f);
    }
    
    // First two entries are 0 (from lookup_tables.py: lfo_increments[0:2] = 0)
    // But for envelopes, we keep them as calculated
    
    return values;
}

std::array<uint16_t, 256> EnvelopeCurves::generateDrumEnvIncrements(
    Sample maxTime, Sample gamma, Sample controlRate) {
    
    std::array<uint16_t, 256> values;
    
    Sample minIncrement = 65536.0f / (maxTime * controlRate);
    Sample maxIncrement = 65536.0f / (4.0f / controlRate); // min_time = 4/controlRate
    
    Sample start = std::pow(maxIncrement, -gamma);
    Sample end = std::pow(minIncrement, -gamma);
    
    for (size_t i = 0; i < 256; ++i) {
        Sample t = static_cast<Sample>(i) / 255.0f;
        Sample rate = start + t * (end - start);
        Sample val = std::pow(rate, -1.0f / gamma);
        values[i] = static_cast<uint16_t>(val + 0.5f);
    }
    
    return values;
}

std::array<uint16_t, 256> EnvelopeCurves::generateGlideIncrements(
    Sample maxTime, Sample gamma, Sample controlRate) {
    
    std::array<uint16_t, 256> values;
    
    Sample minIncrement = 65536.0f / (maxTime * controlRate);
    Sample maxIncrement = 65536.0f / (3.0f / controlRate); // min_time = 3/controlRate
    
    Sample start = std::pow(maxIncrement, -gamma);
    Sample end = std::pow(minIncrement, -gamma);
    
    for (size_t i = 0; i < 256; ++i) {
        Sample t = static_cast<Sample>(i) / 255.0f;
        Sample rate = start + t * (end - start);
        Sample val = std::pow(rate, -1.0f / gamma);
        values[i] = static_cast<uint16_t>(val + 0.5f);
    }
    
    return values;
}

std::array<uint32_t, 256> EnvelopeCurves::generateLfoIncrements(
    Sample minFreq, Sample maxFreq, Sample controlRate) {
    
    std::array<uint32_t, 256> values;
    
    Sample minIncrement = 65536.0f * 65536.0f * minFreq / controlRate;
    Sample maxIncrement = 65536.0f * 65536.0f * maxFreq / controlRate;
    
    // Linear in log space
    Sample logMin = std::log(minIncrement);
    Sample logMax = std::log(maxIncrement);
    
    for (size_t i = 0; i < 256; ++i) {
        Sample t = static_cast<Sample>(i) / 255.0f;
        Sample logVal = logMin + t * (logMax - logMin);
        Sample val = std::exp(logVal);
        values[i] = static_cast<uint32_t>(val + 0.5f);
    }
    
    // First two entries = 0 (from lookup_tables.py)
    values[0] = 0;
    values[1] = 0;
    
    return values;
}

} // namespace dsp
} // namespace lxx