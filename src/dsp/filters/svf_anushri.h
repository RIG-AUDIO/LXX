#pragma once

#include "../core/dsp_types.h"
#include "ssm2164_model.h"
#include <array>

namespace lxx {
namespace dsp {

/**
 * Anushri SVF (State Variable Filter) - Linear Core
 *
 * TPT SVF (Cytomic / Andrew Simper topology-preserving transform):
 *   v0 = in - ic2eq
 *   v1 = (ic1eq + g*v0) / (1 + g*(g + k))
 *   ic1eq = 2*v1 - ic1eq;  v2 = ic2eq + g*v1;  ic2eq = 2*v2 - ic2eq
 *   lp = v2;  bp = v1;  hp = v0 - k*v1 - v2
 *
 *   g = tan(pi*fc/fs);  k = 1/Q (TPT damping). Do not run frequency-response
 *   Q fitting on the audio thread — that was ~10^5 trig evals per cutoff update.
 *   resonance >= 1 => k = 0 (unit-circle poles, sustained self-oscillation).
 *   Cutoff is clamped to 0.45 * fs (Nyquist guard) so tan() stays finite.
 *   Integrator state is clamped and sanitized if non-finite.
 *
 * Resonance knob (0–1): Q = 1/(1-resonance). Stock RES pot is analog-only; only the digital resonance law is modelled.
 *
 * authenticMode_ is a plugin scope flag; it does not change coefficient math.
 */

struct SVFAnushriCalibration {
    // SSM2164 model
    SSM2164Calibration ssm2164;
    
    // Initial state
    Sample initialState[3] = { 0.0f, 0.0f, 0.0f }; // lp, bp, hp
};

// Filter output types (LP/BP/HP panel switch on stock hardware)
enum class SVFOutput : uint8_t {
    LOWPASS = 0,
    BANDPASS = 1,
    HIGHPASS = 2
};

class SVFAnushriLinear {
public:
    SVFAnushriLinear(Sample sampleRate = kSampleRate);
    
    void setSampleRate(Sample sampleRate);
    void reset();
    void setInitialState(const Sample state[3]);
    
    // Set cutoff frequency directly (Hz)
    void setCutoffFreq(Sample freqHz);
    
    // Set cutoff via control voltages (control-voltage form)
    // cv_dac: DAC voltage (0-5V, center 2.048V)
    // cv_input: External CV input (0-5V)
    // cv_pot: Potentiometer voltage (0-5V)
    void setCutoffCV(Sample cv_dac, Sample cv_input, Sample cv_pot);
    
    // Set resonance (Q) via control voltage or normalized value
    // cv: Resonance control voltage at SSM2164 pin
    // or use normalized value 0-1 (mapped via resonanceGain)
    void setResonanceCV(Sample cv);
    void setResonance(Sample value); // 0-1 normalized
    
    // LP/BP/HP mode (panel switch on hardware; process() always returns all three)
    void setFilterMode(SVFOutput mode) { filterMode_ = mode; }
    SVFOutput getFilterMode() const { return filterMode_; }
    Sample processSelected(Sample input);
    
    // Plugin scope: stock panel behavior vs extended conveniences (not coefficient math)
    void setAuthenticMode(bool enabled);
    bool isAuthenticMode() const { return authenticMode_; }
    
    // Use Anushri alternative resonance curve
    void setUseAnuResonance(bool useAnu) { useAnuResonance_ = useAnu; }
    
    // Process one sample - returns LP, BP, HP
    std::array<Sample, 3> process(Sample input);
    
    // Process block
    void processBlock(const Sample* input, Sample* lpOut, Sample* bpOut, Sample* hpOut, size_t numFrames);
    
    // Get current coefficients/state for analysis
    Sample getCutoffFreq() const { return cutoffFreq_; }
    Sample getResonance() const { return resonance_; }
    Sample getG() const { return g_; }       // TPT integrator coefficient tan(pi*fc/fs)
    Sample getK() const { return k_; }       // damping (1/Q at small signal)
    Sample getR() const { return r_; }       // debug: 2/k or 2.0 at self-osc
    
    // State access
    Sample getLPState() const { return state_[0]; }
    Sample getBPState() const { return state_[1]; }
    Sample getHPState() const { return state_[2]; }

    static constexpr Sample kNyquistCutoffRatio = 0.45f;
    static constexpr Sample kMaxTptG = 6.3137515147f; // tan(pi * 0.45)
    static constexpr Sample kMaxIntegrator = 8.0f;

private:
    void updateCoefficients();
    
    Sample sampleRate_;
    SSM2164Model ssm2164_;
    
    // Filter parameters
    Sample cutoffFreq_ = 1000.0f;
    Sample resonance_ = 0.5f;  // 0-1 normalized
    bool useAnuResonance_ = false;
    bool authenticMode_ = true;  // Plugin scope flag (oversample / NL cal elsewhere)
    SVFOutput filterMode_ = SVFOutput::LOWPASS;
    
    // SVF coefficients
    Sample g_ = 0.0f;  // TPT integrator coefficient
    Sample k_ = 0.0f;  // damping coefficient
    Sample r_ = 0.0f;  // debug alias: ~2/k at resonance < 1, 2.0 at self-osc
    
    // State: [ic2eq (lp integrator), ic1eq (bp integrator), hp (computed)]
    Sample state_[3] = { 0.0f, 0.0f, 0.0f };
};

/**
 * Single-output SVF for simple use cases
 */
class SVFAnushriSingle {
public:
    SVFAnushriSingle(Sample sampleRate = kSampleRate, SVFOutput output = SVFOutput::LOWPASS);
    
    void setSampleRate(Sample sampleRate);
    void reset();
    
    void setCutoffFreq(Sample freqHz);
    void setCutoffCV(Sample cv_dac, Sample cv_input, Sample cv_pot);
    void setResonance(Sample value);
    void setResonanceCV(Sample cv);
    void setOutputType(SVFOutput output);
    
    Sample process(Sample input);
    void processBlock(const Sample* input, Sample* output, size_t numFrames);

private:
    SVFAnushriLinear svf_;
    SVFOutput outputType_;
};

} // namespace dsp
} // namespace lxx