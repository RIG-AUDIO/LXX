#pragma once

#include "../core/dsp_types.h"
#include <array>

namespace lxx {
namespace dsp {

/**
 * Self-Oscillating 2-Pole Resonator
 * 
 * Uses explicit pole placement on (or slightly outside) the unit circle
 * for true self-oscillation. This replaces the SVF in self-oscillation mode.
 * 
 * Poles at z = r * exp(±j*θ) where θ = 2π*fc/fs
 * For self-oscillation: r = 1.0 (or slightly > 1 for amplitude growth)
 * 
 * Provides LP, BP, HP outputs like the SVF.
 */

class SelfOscillator {
public:
    SelfOscillator(Sample sampleRate = kSampleRate);
    
    void setSampleRate(Sample sampleRate);
    void reset();
    
    void setFrequency(Sample freqHz);
    void setResonance(Sample r);  // 0 to 1, where 1 = self-oscillation
    void setDrive(Sample drive);   // Amplitude limiter threshold
    
    std::array<Sample, 3> process(Sample input);
    void processBlock(const Sample* input, Sample* lpOut, Sample* bpOut, Sample* hpOut, size_t numFrames);
    
    // Accessors
    Sample getFrequency() const { return freqHz_; }
    Sample getResonance() const { return resonance_; }

private:
    Sample sampleRate_;
    Sample freqHz_ = 1000.0f;
    Sample resonance_ = 0.5f;  // 0 to 1
    Sample drive_ = 1.0f;
    
    // Resonator coefficients
    Sample a1_ = 0.0f;  // 2*r*cos(θ)
    Sample a2_ = 0.0f;  // -r²
    Sample b0_, b1_, b2_;  // Numerator coefficients for LP/BP/HP
    
    // State
    Sample y1_ = 0.0f, y2_ = 0.0f;
    
    // Soft clipping for amplitude limiting
    Sample softClip(Sample x) const;
    
    void updateCoefficients();
};

} // namespace dsp
} // namespace lxx