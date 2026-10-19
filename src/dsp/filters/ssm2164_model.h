#pragma once

#include "../core/dsp_types.h"
#include <cmath>

namespace lxx {
namespace dsp {

/**
 * SSM2164 Model — closed-form VCF/VCA control laws
 * 
 * The SSM2164 is a quad VCA/OTA used for both VCF cutoff control
 * and resonance (Q) control in the Anushri VCF.
 * 
 * Key insight: Two different gain curves are used:
 * 1. Cutoff: exp(-cv / 10.4 / (kT))  -- steeper, for frequency
 * 2. Resonance: exp(-cv / 60.0 / (kT))  -- shallower, for Q
 * 
 * The difference in denominators (10.4 vs 60.0) reflects different
 * circuit configurations (different current mirror ratios, etc.)
 */

struct SSM2164Calibration {
    // Thermal voltage at room temperature
    Sample temperature = 300.13f;  // 27°C = 300.13K
    Sample kBoltzmann = 8.617333262e-5f;
    
    // Cutoff control
    Sample cutoffDenom = 10.4f;
    
    // Resonance control  
    Sample resonanceDenom = 60.0f;
    Sample resonanceTemp = 303.13f;  // 30°C = 303.13K
    
    // Alternative Anushri resonance curve
    bool useAnuResonanceCurve = false;
};

class SSM2164Model {
public:
    SSM2164Model(const SSM2164Calibration& cal = SSM2164Calibration());
    
    void setCalibration(const SSM2164Calibration& cal);
    void setTemperature(Sample tempKelvin);
    
    /**
     * Cutoff frequency gain curve
     * VCF gain law: gain = exp(-cv / 10.4 / (kT))
     * cv is the control voltage at the SSM2164 control pin
     */
    Sample cutoffGain(Sample cv) const;
    
    /**
     * Resonance (Q) gain curve
     * AS2164 gain law: gain = exp(-cv / 60.0 / (kT))
     * cv = resonance_value * 5.0 (scaling from UI 0-1 to control voltage)
     */
    Sample resonanceGain(Sample cv) const;
    
    /**
     * Anushri alternative resonance curve
     * Pot taper: gain = 0.5 / (0.5 + value^2 * 10)
     * value is 0-1 normalized resonance parameter
     */
    Sample anuResonanceGain(Sample value) const;
    
    /**
     * Compute cutoff frequency from control voltages
     * VCF cutoff law f_vcf()
     * 
     * @param cv_dac     DAC voltage (0-5V, center 2.048V)
     * @param cv_input   External CV input voltage (0-5V)
     * @param cv_pot     Potentiometer voltage (0-5V, center ~2.5V)
     * @return Frequency in Hz
     */
    Sample computeCutoffFreq(Sample cv_dac, Sample cv_input, Sample cv_pot) const;
    
    /**
     * Max cutoff frequency (at 0V control)
     */
    Sample getMaxFreq() const;
    
    // VCF circuit component values
    struct CircuitParams {
        Sample r_fb = 37300.0f;
        Sample r_ref = 62000.0f;
        Sample r_input = 200000.0f;
        Sample r_pot = 200000.0f;
        Sample r_dac = 49900.0f;
        Sample r_vcf = 33000.0f;
        Sample c_vcf = 220.0e-12f;
    };
    
    void setCircuitParams(const CircuitParams& params);
    const CircuitParams& getCircuitParams() const { return circuit_; }

private:
    SSM2164Calibration cal_;
    CircuitParams circuit_;
    Sample kt_cutoff_;
    Sample kt_resonance_;
    
    void updateDerived();
};

// Utility: thermal voltage
inline Sample thermalVoltage(Sample tempKelvin) {
    return 8.617333262e-5f * tempKelvin;
}

} // namespace dsp
} // namespace lxx