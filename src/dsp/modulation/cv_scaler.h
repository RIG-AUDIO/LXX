#pragma once

#include "../core/dsp_types.h"

namespace lxx {
namespace dsp {

/**
 * CV Scaling utilities
 * 
 * Ported from voice.cc DAC CV generation logic:
 * - VCO: 1V/octave (0.5V/oct in hardware, but 1V/oct in DAC domain)
 * - VCF: 0.5V/octave
 * - VCA: Exponential (AS2164)
 * - PW: Linear
 */

// VCO CV Scaler (1V/octave equivalent)
// Hardware: 0.5V/oct at VCO, but DAC outputs 0-5V mapped to pitch
// Reference: C3 (MIDI 60) = 2.048V = 261.625 Hz
// From voice.cc: pitch += 2048 (0x800 offset), then CLIP_12 (0-4095)

class VcoCvScaler {
public:
    // Calibration data (from system_settings.h defaults)
    struct Calibration {
        Sample vcoCvOffset = 60.0f * 128.0f;     // MIDI 60 << 7
        Sample vcoCvScaleLow = 64000.0f / 3.0f;  // Below C3
        Sample vcoCvScaleHigh = 64000.0f / 3.0f; // Above C3
    };
    
    VcoCvScaler() = default;
    explicit VcoCvScaler(const Calibration& cal);
    
    void setCalibration(const Calibration& cal);
    
    // Convert pitch (in 1/128 semitone units, MIDI 60 = 60*128)
    // to 12-bit DAC value (0-4095)
    Sample pitchToDac(int16_t pitch) const;
    
    // Inverse: DAC value to pitch (for modulation calculations)
    Sample dacToPitch(Sample dac) const;
    
    // Voltage (0-5V) to pitch
    Sample voltToPitch(Sample volts) const;
    
    // Pitch to voltage
    Sample pitchToVolt(Sample pitch) const;

private:
    Calibration cal_;
    Sample invScaleLow_;
    Sample invScaleHigh_;
};

// VCF CV Scaler (0.5V/octave)
// VCF cutoff law: f_vcf = f_max * exp(-cv / (10.4 * kT))
// From voice.cc: cutoff = (cutoff - kVcfCvOffset) * kVcfCvScale >> 16 + 2048

class VcfCvScaler {
public:
    // From voice.cc constants
    static constexpr Sample kVcfCvOffset = 60.0f * 128.0f;  // 60 * 128
    static constexpr Sample kVcfCvScale = 32000.0f / 3.0f;  // 32000/3
    
    VcfCvScaler() = default;
    
    // Convert cutoff (in pitch units, 1/128 semitone) to 12-bit DAC
    Sample cutoffToDac(Sample cutoff) const;
    
    // Convert DAC to cutoff frequency (Hz)
    Sample dacToFreq(Sample dac) const;
    
    // Convert cutoff frequency to DAC
    Sample freqToDac(Sample freq) const;

private:
    // SSM2164 gain curve
    static Sample ssm2164gain(Sample cv, Sample temperature = kRoomTempKelvin);
};

// VCA CV Scaler (Exponential, AS2164)
// AS2164 gain law: gain = exp(-cv / (60 * kT))
// From voice.cc: VCA CV = envelope * volume >> 4

class VcaCvScaler {
public:
    VcaCvScaler() = default;
    
    // Convert linear gain (0-1) to 12-bit DAC value
    Sample gainToDac(Sample gain) const;
    
    // Convert DAC to linear gain
    Sample dacToGain(Sample dac) const;
    
    // AS2164 gain law
    static Sample as2164gain(Sample cv, Sample temperature = kRoomTempKelvin);

private:
    static constexpr Sample kAs2164Denom = 60.0f;  // 60 mV/decade
};

// PW CV Scaler (Linear)
// From voice.cc: PW CV = (mod_env * pw_env_amt + (lfo + 32768) * pw_lfo_amt) >> 2

class PwCvScaler {
public:
    PwCvScaler() = default;
    
    // Convert pulse width (0-1) to 12-bit DAC
    Sample pwToDac(Sample pw) const;
    
    // Convert DAC to pulse width
    Sample dacToPw(Sample dac) const;
};

// Generic 1V/octave scaler
class VoltPerOctaveScaler {
public:
    VoltPerOctaveScaler(Sample refVolts = 2.048f, Sample refFreq = 440.0f, 
                        Sample refNote = 69.0f);
    
    Sample voltsToFreq(Sample volts) const;
    Sample freqToVolts(Sample freq) const;
    Sample noteToVolts(Sample note) const;
    Sample voltsToNote(Sample volts) const;

private:
    Sample refVolts_;
    Sample refFreq_;
    Sample refNote_;
};

// Generic 0.5V/octave scaler
class HalfVoltPerOctaveScaler {
public:
    HalfVoltPerOctaveScaler(Sample refVolts = 2.048f, Sample refFreq = 440.0f,
                            Sample refNote = 69.0f);
    
    Sample voltsToFreq(Sample volts) const;
    Sample freqToVolts(Sample freq) const;

private:
    Sample refVolts_;
    Sample refFreq_;
    Sample refNote_;
};

} // namespace dsp
} // namespace lxx