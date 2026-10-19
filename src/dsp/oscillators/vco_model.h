#pragma once

#include "../core/dsp_types.h"
#include <cmath>

namespace lxx {
namespace dsp {

/**
 * VCO Model - Model consistent with the upstream Anushri VCO hardware simulation
 * 
 * Models the SSM2164-based VCO with 1V/octave CV control
 * and temperature-dependent behavior.
 * 
 * Key VCO control formulas:
 * - ssm2164gain(cv, temperature) = exp(-cv / 10.4 / (temperature * k))
 * - f_vco(cv_dac, cv_input, temperature) = f_max * ssm2164gain(v1, temperature)
 * - v1 = -ssm2164gain(V_tempco, temperature) * r_fb * i1
 * - i1 = cv_dac / r_dac + cv_input / r_input - 4.096 / r_shift
 * 
 * The VCO frequency is exponential with respect to control voltage.
 */

struct VcoCalibration {
    // Hardware component values
    Sample r_dac = 100000.0f;
    Sample r_shift = 72133.0f;
    Sample r_input = 200000.0f;
    Sample r_fb = 101733.0f;
    Sample r_vco = 22000.0f;
    Sample c_vco = 2.2e-9f;
    Sample r_tempco = 68400.0f;
    Sample r_div = 460.0f;
    Sample r_in = 4320.0f;
    Sample v_ref = 4.110f;
    
    // Derived
    Sample ratio = r_div / (r_in + r_div);
    Sample v_tempco = r_div / (r_div + r_in + r_tempco) * v_ref / ratio;
    
    // Calibration offsets (matching system_settings defaults)
    Sample vcoCvOffset = 60.0f * 128.0f;      // MIDI 60 << 7
    Sample vcoCvScaleLow = 64000.0f / 3.0f;
    Sample vcoCvScaleHigh = 64000.0f / 3.0f;
    
    // Temperature in Kelvin (default 25°C = 298.15K)
    Sample temperature = 298.15f;
};

class VcoModel {
public:
    VcoModel();
    
    void setCalibration(const VcoCalibration& cal);
    void setTemperature(Sample tempKelvin);
    void setDriftAmount(Sample amount);  // 0-100%, 0 = no drift, 1 = full thermal drift
    
    // Main interface: pitch (1/128 semitone units, MIDI 60 = 7680) -> frequency in Hz
    Sample pitchToFreq(int16_t pitch) const;
    
    // CV voltage (0-5V) -> frequency in Hz
    Sample cvToFreq(Sample cvVolts) const;
    
    // Inverse: frequency -> CV voltage
    Sample freqToCv(Sample freq) const;
    
    // MIDI note -> frequency (with drift applied)
    Sample noteToFreq(Sample note) const;
    
    // Get current frequency for a given pitch (includes drift)
    Sample getFrequency(int16_t pitch) const;
    
    // Get thermal drift in cents at current temperature
    Sample getThermalDriftCents(int16_t pitch) const;
    
    // SSM2164 gain function
    static Sample ssm2164gain(Sample cv, Sample temperature);
    
    // Max frequency (at 0V control)
    Sample getMaxFreq() const;

private:
    VcoCalibration cal_;
    Sample driftAmount_ = 0.3f;  // Default 30% - musically tasteful
    Sample kt_;  // k * temperature
    
    void updateDerived();
    
    // Boltzmann constant in eV/K
    static constexpr Sample kBoltzmann = 8.617333262e-5f;
};

} // namespace dsp
} // namespace lxx