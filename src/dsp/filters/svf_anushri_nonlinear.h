#pragma once

#include "svf_anushri.h"
#include "../core/dsp_types.h"
#include <array>

namespace lxx {
namespace dsp {

/**
 * SVF Non-Linear Stage
 *
 * OTA saturation modeled in normalized audio units:
 * - Input knee at kOtaInputSaturationThreshold (~full scale)
 * - Integrator output soft limit at kOtaSaturationThreshold (matches self_oscillator.cpp)
 * - Resonance NL: gentle k increase at high BP level, faded by (1 - resonance)
 *
 * Physical Vt (~26 mV) is NOT applied directly to normalized ±1.0 audio.
 */

struct SVFNonlinearCalibration {
    // Reference thermal voltage (physical); use scaled thresholds below for audio
    Sample thermalVoltage = kThermalVoltage25C;

    // Saturation thresholds in normalized audio units
    Sample inputSaturationThreshold = kOtaInputSaturationThreshold;
    Sample integratorSaturationThreshold = kOtaSaturationThreshold;

    Sample positiveSaturation = 1.0f;
    Sample negativeSaturation = 1.0f;

    bool enableInputSaturation = true;
    bool enableIntegratorSaturation = true;
    bool enableResonanceNonlinearity = true;
};

class SVFAnushriNonlinear {
public:
    SVFAnushriNonlinear(Sample sampleRate = kSampleRate);

    void setSampleRate(Sample sampleRate);
    void setCalibration(const SVFNonlinearCalibration& cal);

    void setCutoffFreq(Sample freqHz);
    void setCutoffCV(Sample cv_dac, Sample cv_input, Sample cv_pot);
    void setResonance(Sample value);
    void setResonanceCV(Sample cv);
    void setUseAnuResonance(bool useAnu);
    void setFilterMode(SVFOutput mode);
    void setAuthenticMode(bool enabled);

    std::array<Sample, 3> process(Sample input);
    void processBlock(const Sample* input, Sample* lpOut, Sample* bpOut, Sample* hpOut, size_t numFrames);

    const SVFAnushriLinear& getLinearCore() const { return svfLinear_; }
    SVFAnushriLinear& getLinearCore() { return svfLinear_; }

    Sample getLPState() const { return state_[0]; }
    Sample getBPState() const { return state_[1]; }
    Sample getHPState() const { return state_[2]; }

    void reset();

private:
    void processNonlinear(Sample input, Sample& lp, Sample& bp, Sample& hp);

    SVFAnushriLinear svfLinear_;
    SVFNonlinearCalibration cal_;

    Sample state_[3] = { 0.0f, 0.0f, 0.0f };
};

} // namespace dsp
} // namespace lxx
