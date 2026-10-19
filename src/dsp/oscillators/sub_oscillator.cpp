#include "sub_oscillator.h"
#include <cmath>

namespace lxx {
namespace dsp {

SubOscillator::SubOscillator(Sample sampleRate) : sampleRate_(sampleRate) {
    reset();
}

void SubOscillator::setSampleRate(Sample sampleRate) {
    sampleRate_ = sampleRate;
    setFrequency(freqHz_);
}

void SubOscillator::reset() {
    phase_ = 0.0f;
    value_ = 1.0f;
    setFrequency(freqHz_);
}

void SubOscillator::setFrequency(Sample freqHz) {
    freqHz_ = clamp(freqHz, 0.001f, sampleRate_ * 0.25f);
    phaseInc_ = freqHz_ / sampleRate_;
}

void SubOscillator::hardSync() {
    phase_ = 0.0f;
    value_ = 1.0f;
}

Sample SubOscillator::blep(Sample t) {
    // PolyBLEP for square wave edges
    if (t < 1.0f) {
        return t - t * t * 0.5f - 0.5f;
    }
    return 0.0f;
}

Sample SubOscillator::process() {
    // Square wave: +1 for first half, -1 for second half
    Sample square = (phase_ < 0.5f) ? 1.0f : -1.0f;
    
    // Apply BLEP at edges
    Sample t = phase_ / phaseInc_;
    Sample blep = 0.0f;
    
    // Rising edge at phase 0
    if (t < 1.0f) {
        blep += 2.0f * (t - t * t * 0.5f - 0.5f);
    }
    
    // Falling edge at phase 0.5
    Sample t_half = (phase_ - 0.5f) / phaseInc_;
    if (t_half >= 0.0f && t_half < 1.0f) {
        blep -= 2.0f * (t_half - t_half * t_half * 0.5f - 0.5f);
    }
    
    phase_ += phaseInc_;
    if (phase_ >= 1.0f) phase_ -= 1.0f;
    
    value_ = square + blep;
    return value_;
}

void SubOscillator::processBlock(Sample* output, size_t numFrames) {
    for (size_t i = 0; i < numFrames; ++i) {
        output[i] = process();
    }
}

} // namespace dsp
} // namespace lxx