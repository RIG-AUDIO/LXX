#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <limits>

namespace lxx {
namespace dsp {

// Sample type - float32 for DSP, double for critical accumulators
using Sample = float;
using Accumulator = double;

// Constants
constexpr Sample kSampleRate = 48000.0f;
constexpr Sample kTwoPi = 6.28318530717958647692f;
constexpr Sample kPi = 3.14159265358979323846f;
constexpr Sample kInvTwoPi = 0.15915494309189533576f;
constexpr Sample kInvSampleRate = 1.0f / kSampleRate;

// Oversampling factors
constexpr int kOversampleFactor2x = 2;
constexpr int kOversampleFactor4x = 4;

// MIDI constants
constexpr int kMidiNoteMin = 0;
constexpr int kMidiNoteMax = 127;
constexpr Sample kMidiA4Freq = 440.0f;
constexpr int kMidiA4Note = 69;

// Voltage ranges (matching original hardware)
constexpr Sample kCVRangeMin = 0.0f;
constexpr Sample kCVRangeMax = 5.0f;
constexpr Sample kCVRangeCenter = 2.5f;

// 1V/octave scaling
constexpr Sample kVoltsPerOctave = 1.0f;
constexpr Sample kSemitonesPerVolt = 12.0f;
constexpr Sample kCentsPerVolt = 1200.0f;

// 0.5V/octave (filter tracking)
constexpr Sample kHalfVoltsPerOctave = 0.5f;

// OTA soft-limit threshold in normalized audio units (±1.0 = full scale).
// Matches self_oscillator.cpp (0.2); ~200 mV equivalent in 1 V nominal headroom.
// Physical Vt (~26 mV) applies to differential input in VOLTS — do not apply
// kThermalVoltage25C directly to normalized audio without this scale.
constexpr Sample kOtaSaturationThreshold = 0.2f;
constexpr Sample kOtaInputSaturationThreshold = 1.0f;
constexpr Sample kThermalVoltage25C = 0.02569f;
constexpr Sample kRoomTempKelvin = 298.15f;

// SSM2164/AS2164 constants
constexpr Sample kSSM2164GainDenom = 10.4f;  // V/octave denominator
constexpr Sample kAS2164GainDenom = 60.0f;   // VCA denominator (60 mV/decade)

// Envelope time constants (from lookup_tables.py)
constexpr Sample kEnvMaxTime = 12.0f;      // seconds
constexpr Sample kEnvMinTime = 3.0f / (20000000.0f / 510.0f / 8.0f / 2.0f); // control rate
constexpr Sample kEnvGamma = 0.175f;

// Drum envelope constants
constexpr Sample kDrmEnvMaxTime = 4.0f;
constexpr Sample kDrmEnvGamma = 0.25f;

// Glide constants
constexpr Sample kGlideMaxTime = 6.0f;
constexpr Sample kGlideGamma = 0.1f;

// LFO constants
constexpr Sample kLfoMinFreq = 1.0f / 16.0f;
constexpr Sample kLfoMaxFreq = 100.0f;

// Inline math utilities
inline Sample clamp(Sample x, Sample lo, Sample hi) {
    return std::max(lo, std::min(hi, x));
}

inline Sample lerp(Sample a, Sample b, Sample t) {
    return a + t * (b - a);
}

inline Sample smoothstep(Sample x) {
    x = clamp(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

inline Sample smootherstep(Sample x) {
    x = clamp(x, 0.0f, 1.0f);
    return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
}

// Fast approx exp for real-time use
inline Sample fast_exp(Sample x) {
    // Schraudolph 1999 approximation
    union { int32_t i; Sample f; } v;
    v.i = int32_t((1 << 23) * (1.4426950409f * x + 126.94269504f));
    return v.f;
}

// Fast approx log2
inline Sample fast_log2(Sample x) {
    union { int32_t i; Sample f; } v;
    v.f = x;
    return (v.i - (127 << 23)) * (1.0f / (1 << 23));
}

// MIDI note to frequency (equal temperament)
inline Sample midiToFreq(int note) {
    return kMidiA4Freq * std::pow(2.0f, (note - kMidiA4Note) / 12.0f);
}

// Frequency to MIDI note (continuous)
inline Sample freqToMidi(Sample freq) {
    return kMidiA4Note + 12.0f * std::log2(freq / kMidiA4Freq);
}

// Voltage to frequency (1V/octave)
inline Sample voltPerOctToFreq(Sample volts, Sample refFreq = kMidiA4Freq, Sample refVolts = 2.048f) {
    return refFreq * std::pow(2.0f, (volts - refVolts) * kSemitonesPerVolt);
}

// Voltage to frequency (0.5V/octave)
inline Sample halfVoltPerOctToFreq(Sample volts, Sample refFreq = 440.0f, Sample refVolts = 2.048f) {
    return refFreq * std::pow(2.0f, (volts - refVolts) * (kSemitonesPerVolt * 0.5f));
}

// Linear interpolation for lookup tables
inline Sample interpolateTable(const Sample* table, size_t tableSize, Sample index) {
    if (index <= 0.0f) return table[0];
    if (index >= tableSize - 1) return table[tableSize - 1];
    
    size_t i = static_cast<size_t>(index);
    Sample frac = index - static_cast<Sample>(i);
    return lerp(table[i], table[i + 1], frac);
}

// Denormal protection
inline void denormalize(Sample& x) {
    constexpr Sample kDenormalThreshold = 1e-20f;
    if (std::abs(x) < kDenormalThreshold) x = 0.0f;
}

inline void denormalize(Sample* buffer, size_t n) {
    for (size_t i = 0; i < n; ++i) denormalize(buffer[i]);
}

} // namespace dsp
} // namespace lxx