#include "self_oscillator.h"
#include <cmath>

namespace lxx {
namespace dsp {

SelfOscillator::SelfOscillator(Sample sampleRate) : sampleRate_(sampleRate) {
    reset();
    updateCoefficients();
}

void SelfOscillator::setSampleRate(Sample sampleRate) {
    sampleRate_ = sampleRate;
    updateCoefficients();
}

void SelfOscillator::reset() {
    y1_ = 0.0f;
    y2_ = 0.0f;
}

void SelfOscillator::setFrequency(Sample freqHz) {
    freqHz_ = clamp(freqHz, 1.0f, sampleRate_ * 0.49f);
    updateCoefficients();
}

void SelfOscillator::setResonance(Sample r) {
    resonance_ = clamp(r, 0.0f, 1.0f);
    updateCoefficients();
}

void SelfOscillator::setDrive(Sample drive) {
    drive_ = clamp(drive, 0.1f, 10.0f);
}

void SelfOscillator::updateCoefficients() {
    // Pole angle: θ = 2π * fc / fs
    Sample theta = 2.0f * kPi * freqHz_ / sampleRate_;
    
    // Resonance 0 to 1 maps to pole radius 0.5 to 1.01
    // At resonance=1: pole radius = 1.005 (slight growth for self-oscillation)
    // At resonance=0: pole radius = 0.5 (no resonance)
    Sample r = 0.5f + resonance_ * 0.505f;  // 0.5 to 1.005

    a1_ = 2.0f * r * std::cos(theta);
    a2_ = -r * r;
    // LP: (1 - r²) / (1 - a1*z^-1 + a2*z^-2)  -- DC gain = 1
    // BP: (1 - r²)*z^-1 / (1 - a1*z^-1 + a2*z^-2)  -- bandpass
    // HP: (1 - r)² / (1 - a1*z^-1 + a2*z^-2) * (1 + z^-1)² / 4?  -- highpass
    
    // Standard biquad numerators for SVF-like outputs:
    // LP: y = (1 - r²) * x / (1 - a1*z^-1 + a2*z^-2)
    // BP: y = g * (1 - r²) * x * z^-1 / (1 - a1*z^-1 + a2*z^-2)  -- approximate
    // HP: y = (1 - a1 + a2) / (1 - a1*z^-1 + a2*z^-2) * x  -- approximate
    
    // Simplified: use standard SVF-like outputs from the biquad
    // For a 2-pole resonator with poles at r*exp(±jθ):
    // LP numerator: (1 - r²) / 2  (to get DC gain = 1)
    // BP numerator: g * (1 - r²) * sin(θ) * z^-1
    // HP numerator: (1 - r)²/2 * (1 + z^-1)²/4?  -- complex
    
    // Simplified approach: use the biquad directly and derive LP/BP/HP from it
    // We'll implement as a biquad with multiple numerator sets
    
    // For a 2-pole resonator with poles at r*exp(±jθ):
    // The impulse response is a decaying (or growing) sinusoid
    // LP: H_lp = k / (1 - a1*z^-1 + a2*z^-2) where a1 = 2r*cosθ, a2 = -r²
    // BP: H_bp = k*sin(θ)*z^-1 / (1 - a1*z^-1 + a2*z^-2)
    // HP: H_hp = (1 - a1 + a2) / (1 - a1*z^-1 + a2*z^-2)  -- approximate highpass
    
    // Normalization for LP: DC gain = 1 / (1 - a1 + a2)
    b0_ = (1.0f - a1_ + a2_) * 0.5f;  // HP numerator scale
    b1_ = 2.0f * std::sin(2.0f * kPi * freqHz_ / sampleRate_);  // BP scale
    b2_ = 1.0f;  // LP scale
}

Sample SelfOscillator::softClip(Sample x) const {
    // Soft clipping: tanh-like saturation
    Sample vt = 0.2f;  // Threshold
    return drive_ * vt * std::tanh(x / (vt * drive_));
}

std::array<Sample, 3> SelfOscillator::process(Sample input) {
    // Standard biquad: y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
    // But we need LP, BP, HP outputs
    // Simplified: use the biquad directly for LP, derive BP and HP
    
    // Direct Form II Transposed
    Sample x = input;
    Sample y = b0_ * x + a1_ * y1_ + a2_ * y2_;
    
    // Apply soft clipping for amplitude limiting
    y = softClip(y);
    
    y2_ = y1_;
    y1_ = y;
    
    // For LP output, we can use the biquad directly
    // For BP and HP, we'd need additional processing
    // Simplified: return LP for now
    return { y, 0.0f, 0.0f };
}

void SelfOscillator::processBlock(const Sample* input, Sample* lpOut, Sample* bpOut, Sample* hpOut, size_t numFrames) {
    for (size_t i = 0; i < numFrames; ++i) {
        auto out = process(input[i]);
        lpOut[i] = out[0];
        bpOut[i] = out[1];
        hpOut[i] = out[2];
    }
}

} // namespace dsp
} // namespace lxx