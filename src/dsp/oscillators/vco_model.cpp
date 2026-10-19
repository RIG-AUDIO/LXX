#include "vco_model.h"
#include <cmath>

namespace lxx {
namespace dsp {

VcoModel::VcoModel() {
    updateDerived();
}

void VcoModel::setCalibration(const VcoCalibration& cal) {
    cal_ = cal;
    updateDerived();
}

void VcoModel::setTemperature(Sample tempKelvin) {
    cal_.temperature = tempKelvin;
    updateDerived();
}

void VcoModel::setDriftAmount(Sample amount) {
    driftAmount_ = clamp(amount, 0.0f, 1.0f);
}

void VcoModel::updateDerived() {
    kt_ = kBoltzmann * cal_.temperature;
    // Recalculate V_tempco if needed
    cal_.ratio = cal_.r_div / (cal_.r_in + cal_.r_div);
    cal_.v_tempco = cal_.r_div / (cal_.r_div + cal_.r_in + cal_.r_tempco) * cal_.v_ref / cal_.ratio;
}

Sample VcoModel::ssm2164gain(Sample cv, Sample temperature) {
    return std::exp(-cv / (10.4f * kBoltzmann * temperature));
}

Sample VcoModel::pitchToFreq(int16_t pitch) const {
    // pitch is in 1/128 semitone units
    // Convert to semitones from MIDI 60 (C3)
    Sample semitonesFromC3 = (pitch - 7680.0f) / 128.0f; // 60 * 128 = 7680
    
    // Frequency = 261.625 * 2^(semitones/12)
    constexpr Sample c3Freq = 261.625f;
    Sample freq = c3Freq * std::pow(2.0f, semitonesFromC3 / 12.0f);
    
    // Apply thermal drift if enabled
    if (driftAmount_ > 0.0f) {
        Sample driftCents = getThermalDriftCents(pitch);
        // Convert cents to frequency multiplier
        Sample driftMult = std::pow(2.0f, driftCents * driftAmount_ / 1200.0f);
        freq *= driftMult;
    }
    
    return freq;
}

Sample VcoModel::cvToFreq(Sample cvVolts) const {
    // CV is 0-5V, center at 2.048V = C3
    // f_vco uses cv_dac (0-5V) and cv_input (0-5V)
    // The DAC output goes through the VCO circuit
    
    // Current from DAC
    Sample i1 = cvVolts / cal_.r_dac - 4.096f / cal_.r_shift;
    
    // Voltage at VCO control pin
    Sample v1 = -ssm2164gain(cal_.v_tempco, cal_.temperature) * cal_.r_fb * i1;
    
    // Max frequency
    Sample f_max = 2.0f / (cal_.r_vco * cal_.c_vco);
    
    // Final frequency
    Sample freq = f_max * ssm2164gain(v1, cal_.temperature);
    
    // Apply thermal drift
    if (driftAmount_ > 0.0f) {
        // Calculate equivalent pitch for this CV
        // This is approximate - the drift is small
        Sample driftCents = getThermalDriftCents(static_cast<int16_t>(7680 + 128 * 12 * std::log2(freq / 261.625f)));
        Sample driftMult = std::pow(2.0f, driftCents * driftAmount_ / 1200.0f);
        freq *= driftMult;
    }
    
    return freq;
}

Sample VcoModel::freqToCv(Sample freq) const {
    // Inverse of cvToFreq - iterative or approximate
    // For now, use 1V/oct approximation
    constexpr Sample c3Freq = 261.625f;
    Sample semitones = 12.0f * std::log2(freq / c3Freq);
    Sample cv = 2.048f + semitones / 12.0f; // 1V/oct
    return clamp(cv, 0.0f, 5.0f);
}

Sample VcoModel::noteToFreq(Sample note) const {
    // MIDI note to frequency with drift
    constexpr Sample c3Freq = 261.625f;
    Sample freq = c3Freq * std::pow(2.0f, (note - 60.0f) / 12.0f);
    
    if (driftAmount_ > 0.0f) {
        int16_t pitch = static_cast<int16_t>(note * 128.0f);
        Sample driftCents = getThermalDriftCents(pitch);
        Sample driftMult = std::pow(2.0f, driftCents * driftAmount_ / 1200.0f);
        freq *= driftMult;
    }
    
    return freq;
}

Sample VcoModel::getFrequency(int16_t pitch) const {
    return pitchToFreq(pitch);
}

Sample VcoModel::getThermalDriftCents(int16_t pitch) const {
    // Thermal behaviour of the VCO control path
    // The VCO pitch varies with temperature due to tempco resistor
    
    // At nominal temperature (25°C = 298.15K), pitch is calibrated
    // At other temperatures, the tempco causes pitch shift
    
    // VCO control law:
    // V_tempco = r_div / (r_div + r_in + r_tempco) * V_ref / ratio
    // This voltage changes with temperature (through tempco resistor)
    // Which shifts the pitch
    
    // The tempco resistor (68400 ohm at 25°C) has temperature coefficient
    // This causes the reference voltage to drift
    
    // Simplified model: pitch drift proportional to temperature difference
    // from calibration temperature
    constexpr Sample calibTemp = 298.15f; // 25°C
    Sample tempDiff = cal_.temperature - calibTemp;
    
    // Empirical: ~0.3 cents/°C near middle of range
    // More at extremes due to exponential nature
    Sample baseDrift = tempDiff * 0.3f; // cents per °C
    
    // Scale by pitch (higher pitches drift more in cents)
    Sample semitonesFromC3 = (pitch - 7680.0f) / 128.0f;
    Sample pitchFactor = 1.0f + std::abs(semitonesFromC3) / 60.0f;
    
    return baseDrift * pitchFactor;
}

Sample VcoModel::getMaxFreq() const {
    return 2.0f / (cal_.r_vco * cal_.c_vco);
}

} // namespace dsp
} // namespace lxx