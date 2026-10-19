#include "blep_oscillator.h"
#include <cmath>
#include <algorithm>

namespace lxx {
namespace dsp {

// MinBLEP table - precomputed for high-quality anti-aliasing
const Sample BlePOscillator::kMinBlepTable[kMinBlepTableSize] = { 0 }; // Placeholder

BlePOscillator::BlePOscillator(Sample sampleRate) : sampleRate_(sampleRate) {
    reset();
}

void BlePOscillator::setSampleRate(Sample sampleRate) {
    sampleRate_ = sampleRate;
    updatePhaseIncrement();
}

void BlePOscillator::reset() {
    phase_ = 0.0f;
    nextSample_ = 0.0f;
    integratorState_ = 0.0f;
    subPhase_ = 0.0f;
    subPhaseInc_ = 0.0f;
    updatePhaseIncrement();
}

void BlePOscillator::setWaveform(Waveform waveform) {
    waveform_ = waveform;
}

void BlePOscillator::setFrequency(Sample freqHz) {
    freqHz_ = clamp(freqHz, 0.001f, sampleRate_ * 0.5f);
    phaseInc_ = freqHz_ / sampleRate_;
}

void BlePOscillator::updatePhaseIncrement() {
    phaseInc_ = freqHz_ / sampleRate_;
}

void BlePOscillator::setPulseWidth(Sample pw) {
    pulseWidth_ = clamp(pw, 0.001f, 0.999f);
}

void BlePOscillator::setPhase(Sample phase) {
    phase_ = std::fmod(phase, 1.0f);
    if (phase_ < 0.0f) phase_ += 1.0f;
}

void BlePOscillator::hardSync() {
    phase_ = 0.0f;
}

/**
 * PolyBLEP for step discontinuity
 * Returns the correction to add to the signal at normalized time t through the step
 * t = 0 at the step, t = 1 one sample later
 * The step goes from 0 to 1 (unit step)
 */
inline Sample polyblep_step(Sample t) {
    if (t < 1.0f) {
        // Integral of (1 - t) from 0 to t: t - t^2/2
        // Unit step response: 0 for t<0, 1 for t>1
        // Correction: t - t^2/2 - 0.5 (centered at 0)
        return t - t * t * 0.5f - 0.5f;
    }
    return 0.0f;
}

/**
 * PolyBLEP for slope discontinuity (ramp step)
 * Used for triangle wave which is integral of square
 * The slope changes by 'slope_delta' at the discontinuity
 */
inline Sample polyblep_ramp(Sample t, Sample slope_delta) {
    if (t < 1.0f) {
        // Integral of step is ramp: t^2/2 - t^3/6
        // Slope change of 'slope_delta' means correction = slope_delta * (t^2/2 - t^3/6 - 1/3)
        // Centered so average is zero
        return slope_delta * (t * t * 0.5f - t * t * t / 6.0f - 1.0f / 3.0f);
    }
    return 0.0f;
}

/**
 * Standard PolyBLEP for sawtooth (value discontinuity)
 */
inline Sample polyblep_saw(Sample t) {
    if (t < 1.0f) {
        return t * t * 0.5f + t - 1.0f;
    }
    return 0.0f;
}

Sample BlePOscillator::process() {
    Sample output = 0.0f;
    
    switch (waveform_) {
        case Waveform::TRIANGLE:
            output = processTriangle();
            break;
        case Waveform::SAWTOOTH:
            output = processSawtooth();
            break;
        case Waveform::PULSE:
            output = processPulse();
            break;
        case Waveform::SUB_SQUARE:
            output = processSubSquare();
            break;
    }
    
    return output;
}

Sample BlePOscillator::processTriangle() {
    // Triangle wave = integral of square wave
    // Generate square wave with BLEP, then integrate with leaky integrator
    
    // Square wave value (+1/-1)
    Sample square = (phase_ < 0.5f) ? 1.0f : -1.0f;
    
    // BLEP for square wave edges
    Sample t = phase_ / phaseInc_;
    Sample blep = 0.0f;
    
    // Rising edge at phase 0 (wrap)
    if (t < 1.0f) {
        blep += 2.0f * polyblep_step(t); // step from -1 to +1 = +2
    }
    // Falling edge at phase 0.5
    Sample t_half = (phase_ - 0.5f) / phaseInc_;
    if (t_half >= 0.0f && t_half < 1.0f) {
        blep -= 2.0f * polyblep_step(t_half); // step from +1 to -1 = -2
    }
    
    Sample square_blep = square + blep;
    
    // Leaky integrator: y[n] = y[n-1] + square_blep * phaseInc_ - leak * y[n-1]
    // Leak factor to prevent DC buildup (very small)
    constexpr Sample leak = 1e-6f;
    integratorState_ = integratorState_ * (1.0f - leak) + square_blep * phaseInc_;
    
    // Triangle amplitude scaling (integrator output is approx +/- 0.25)
    // Scale to -1 to +1 range
    Sample triangle = integratorState_ * 4.0f;
    
    phase_ += phaseInc_;
    if (phase_ >= 1.0f) phase_ -= 1.0f;
    
    return triangle;
}

Sample BlePOscillator::processSawtooth() {
    // Sawtooth: -1 to +1 ramp, then step back to -1
    // Value: 2*phase - 1
    Sample saw = phase_ * 2.0f - 1.0f;
    
    // Step at wrap: from +1 to -1 = step of -2
    Sample t = phase_ / phaseInc_;
    Sample blep = 0.0f;
    
    if (t < 1.0f) {
        // Standard PolyBLEP for sawtooth step of -2
        // Correction = step_size * polyblep(t) = -2 * polyblep(t)
        blep = -2.0f * polyblep_step(t);
    }
    
    phase_ += phaseInc_;
    if (phase_ >= 1.0f) phase_ -= 1.0f;
    
    return saw + blep;
}

Sample BlePOscillator::processPulse() {
    // Pulse wave: two steps per cycle
    // Rising edge at phase 0: -1 to +1 (step = +2)
    // Falling edge at pulseWidth: +1 to -1 (step = -2)
    
    Sample t = phase_ / phaseInc_;
    Sample t_pw = (phase_ - pulseWidth_) / phaseInc_;
    
    Sample pulse = (phase_ < pulseWidth_) ? 1.0f : -1.0f;
    Sample blep = 0.0f;
    
    // Rising edge at phase 0 (wrap)
    if (t < 1.0f) {
        blep += 2.0f * polyblep_step(t);
    }
    
    // Falling edge at pulseWidth
    if (t_pw >= 0.0f && t_pw < 1.0f) {
        blep -= 2.0f * polyblep_step(t_pw);
    }
    
    phase_ += phaseInc_;
    if (phase_ >= 1.0f) phase_ -= 1.0f;
    
    return pulse + blep;
}

Sample BlePOscillator::processSubSquare() {
    // Sub-oscillator: square wave at 1/2 frequency; always track current pitch.
    subPhaseInc_ = phaseInc_ * 0.5f;
    if (subPhaseInc_ <= 0.0f)
        return 0.0f;

    Sample square = (subPhase_ < 0.5f) ? 1.0f : -1.0f;

    Sample t = subPhase_ / subPhaseInc_;
    Sample blep = 0.0f;

    // Rising edge at 0
    if (t < 1.0f) {
        blep += 2.0f * polyblep_step(t);
    }
    // Falling edge at 0.5
    Sample t_half = (subPhase_ - 0.5f) / subPhaseInc_;
    if (t_half >= 0.0f && t_half < 1.0f) {
        blep -= 2.0f * polyblep_step(t_half);
    }

    subPhase_ += subPhaseInc_;
    if (subPhase_ >= 1.0f) subPhase_ -= 1.0f;

    return square + blep;
}

void BlePOscillator::processBlock(Sample* output, size_t numFrames) {
    for (size_t i = 0; i < numFrames; ++i) {
        output[i] = process();
    }
}

} // namespace dsp
} // namespace lxx