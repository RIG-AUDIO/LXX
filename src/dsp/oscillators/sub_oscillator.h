#pragma once

#include "../core/dsp_types.h"

namespace lxx {
namespace dsp {

/**
 * Sub-Oscillator
 * 
 * Generates a square wave at 1/2 the main oscillator frequency.
 * Phase-locked to main VCO (hard sync resets sub-phase).
 * Matches original Anushri sub-oscillator behavior.
 */

class SubOscillator {
public:
    SubOscillator(Sample sampleRate = kSampleRate);
    
    void setSampleRate(Sample sampleRate);
    void reset();
    
    // Set frequency (will be half of main VCO freq)
    void setFrequency(Sample freqHz);
    
    // Hard sync - called when main VCO phase resets
    void hardSync();
    
    // Process one sample
    Sample process();
    
    // Process block
    void processBlock(Sample* output, size_t numFrames);
    
    // Get current output level
    Sample getValue() const { return value_; }

private:
    Sample sampleRate_;
    Sample freqHz_ = 440.0f;
    Sample phase_ = 0.0f;
    Sample phaseInc_ = 0.0f;
    Sample value_ = 1.0f;
    
    // BLEP for anti-aliased edges
    Sample blep(Sample t);
};

} // namespace dsp
} // namespace lxx