#include "cv_scaler.h"
#include "../amplifiers/vca_curve.h"
#include "../filters/ssm2164_model.h"
#include <cmath>

namespace lxx {
namespace dsp {

// VcoCvScaler implementation
VcoCvScaler::VcoCvScaler(const Calibration& cal) : cal_(cal) {
    invScaleLow_ = 1.0f / cal_.vcoCvScaleLow;
    invScaleHigh_ = 1.0f / cal_.vcoCvScaleHigh;
}

void VcoCvScaler::setCalibration(const Calibration& cal) {
    cal_ = cal;
    invScaleLow_ = 1.0f / cal_.vcoCvScaleLow;
    invScaleHigh_ = 1.0f / cal_.vcoCvScaleHigh;
}

Sample VcoCvScaler::pitchToDac(int16_t pitch) const {
    // From voice.cc WriteDACStateSample:
    // pitch is in 1/128 semitone units
    // Split at C3 (60 * 128)
    Sample p = static_cast<Sample>(pitch);
    
    if (pitch < 60 * 128) {
        p = (p - cal_.vcoCvOffset) * invScaleLow_;
    } else {
        p = (p - cal_.vcoCvOffset) * invScaleHigh_;
    }
    
    // Add 2048 (0x800) offset and clip to 12-bit
    p += 2048.0f;
    return clamp(p, 0.0f, 4095.0f);
}

Sample VcoCvScaler::dacToPitch(Sample dac) const {
    // Inverse of pitchToDac
    Sample p = clamp(dac, 0.0f, 4095.0f) - 2048.0f;
    
    // We don't know which scale was used, so approximate
    // Use average scale for inverse
    Sample avgScale = (cal_.vcoCvScaleLow + cal_.vcoCvScaleHigh) * 0.5f;
    return p * avgScale + cal_.vcoCvOffset;
}

Sample VcoCvScaler::voltToPitch(Sample volts) const {
    // 0-5V DAC range maps to 0-4095
    // 2.048V = MIDI 60 (C3)
    Sample dac = volts * (4096.0f / 5.0f);
    return dacToPitch(dac);
}

Sample VcoCvScaler::pitchToVolt(Sample pitch) const {
    Sample dac = pitchToDac(static_cast<int16_t>(pitch));
    return dac * (5.0f / 4096.0f);
}

// VcfCvScaler implementation
Sample VcfCvScaler::cutoffToDac(Sample cutoff) const {
    // From voice.cc:
    // cutoff = (cutoff - kVcfCvOffset) * kVcfCvScale >> 16 + 2048
    Sample c = (cutoff - kVcfCvOffset) * kVcfCvScale / 65536.0f;
    c += 2048.0f;
    return clamp(c, 0.0f, 4095.0f);
}

Sample VcfCvScaler::dacToFreq(Sample dac) const {
    // Hardware summing inverts DAC current (v1 = -i1 * r_fb), so
    // higher DAC voltage opens the filter. The previous (volts-2.048) shortcut
    // skipped that inversion and reversed cutoff vs the panel.
    const Sample volts = clamp(dac, 0.0f, 4095.0f) * (5.0f / 4096.0f);
    SSM2164Model model;
    return model.computeCutoffFreq(volts, 0.0f, 0.0f);
}

Sample VcfCvScaler::freqToDac(Sample freq) const {
    SSM2164Model model;
    const auto& circuit = model.getCircuitParams();
    const Sample fMax = model.getMaxFreq();
    const Sample f = clamp(freq, 1.0f, fMax * 0.999f);
    const Sample kt = 8.617333262e-5f * kRoomTempKelvin;
    const Sample v1 = -10.4f * kt * std::log(f / fMax);
    const Sample i1 = -v1 / circuit.r_fb;
    const Sample dacVolts = (i1 + 4.096f / circuit.r_ref) * circuit.r_dac;
    return clamp(dacVolts * (4096.0f / 5.0f), 0.0f, 4095.0f);
}

Sample VcfCvScaler::ssm2164gain(Sample cv, Sample temperature) {
    constexpr Sample k = 8.617333262e-5f;
    return std::exp(-cv / (10.4f * k * temperature));
}

// VcaCvScaler implementation
Sample VcaCvScaler::gainToDac(Sample gain) const {
    // From voice.cc: VCA CV = envelope * volume >> 4
    // envelope is 0-65535, volume is 0-255
    // Result shifted right 4 = 0-4095
    gain = clamp(gain, 0.0f, 1.0f);
    return gain * 4095.0f;
}

Sample VcaCvScaler::dacToGain(Sample dac) const {
    return As2164Curve::dacToGain(dac);
}

Sample VcaCvScaler::as2164gain(Sample cv, Sample temperature) {
    return As2164Curve::gainFromControlVoltage(cv, temperature);
}

// PwCvScaler implementation
Sample PwCvScaler::pwToDac(Sample pw) const {
    // PW is 0-1 (0% to 100% duty cycle)
    // From voice.cc: PW CV = (mod_env * amt + (lfo + 32768) * amt) >> 2
    // Clipped to 12-bit
    pw = clamp(pw, 0.0f, 1.0f);
    return pw * 4095.0f;
}

Sample PwCvScaler::dacToPw(Sample dac) const {
    return clamp(dac, 0.0f, 4095.0f) / 4095.0f;
}

// VoltPerOctaveScaler implementation
VoltPerOctaveScaler::VoltPerOctaveScaler(Sample refVolts, Sample refFreq, Sample refNote)
    : refVolts_(refVolts), refFreq_(refFreq), refNote_(refNote) {}

Sample VoltPerOctaveScaler::voltsToFreq(Sample volts) const {
    return refFreq_ * std::pow(2.0f, (volts - refVolts_) * kSemitonesPerVolt);
}

Sample VoltPerOctaveScaler::freqToVolts(Sample freq) const {
    return refVolts_ + std::log2(freq / refFreq_) / kSemitonesPerVolt;
}

Sample VoltPerOctaveScaler::noteToVolts(Sample note) const {
    return refVolts_ + (note - refNote_) / kSemitonesPerVolt;
}

Sample VoltPerOctaveScaler::voltsToNote(Sample volts) const {
    return refNote_ + (volts - refVolts_) * kSemitonesPerVolt;
}

// HalfVoltPerOctaveScaler implementation
HalfVoltPerOctaveScaler::HalfVoltPerOctaveScaler(Sample refVolts, Sample refFreq, Sample refNote)
    : refVolts_(refVolts), refFreq_(refFreq), refNote_(refNote) {}

Sample HalfVoltPerOctaveScaler::voltsToFreq(Sample volts) const {
    return refFreq_ * std::pow(2.0f, (volts - refVolts_) * (kSemitonesPerVolt * 0.5f));
}

Sample HalfVoltPerOctaveScaler::freqToVolts(Sample freq) const {
    return refVolts_ + std::log2(freq / refFreq_) / (kSemitonesPerVolt * 0.5f);
}

} // namespace dsp
} // namespace lxx