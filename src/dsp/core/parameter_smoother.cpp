#include "parameter_smoother.h"
#include <cmath>

namespace lxx {
namespace dsp {

ParameterSmoother::ParameterSmoother(Sample timeConstantMs, Sample sampleRate) {
    setTimeConstant(timeConstantMs, sampleRate);
}

void ParameterSmoother::reset(Sample initialValue) {
    current_ = initialValue;
    target_ = initialValue;
}

void ParameterSmoother::setTarget(Sample target) {
    target_ = target;
}

void ParameterSmoother::setTimeConstant(Sample timeConstantMs, Sample sampleRate) {
    // 1-pole lowpass: y[n] = (1 - a) * x[n] + a * y[n-1]
    // where a = exp(-1 / (tau * fs)), tau in seconds
    Sample tau = timeConstantMs * 0.001f;
    coeff_ = std::exp(-1.0f / (tau * sampleRate));
    // Clamp for stability
    coeff_ = clamp(coeff_, 0.0f, 0.99999f);
}

void ParameterSmoother::updateCoeff() {
    // No-op, coeff updated in setTimeConstant
}

Sample ParameterSmoother::process() {
    current_ = target_ + coeff_ * (current_ - target_);
    return current_;
}

void ParameterSmoother::processBlock(Sample* buffer, size_t numFrames, Sample target) {
    target_ = target;
    for (size_t i = 0; i < numFrames; ++i) {
        buffer[i] = process();
    }
}

// LogParameterSmoother implementation
LogParameterSmoother::LogParameterSmoother(Sample timeConstantMs, Sample sampleRate) {
    setTimeConstant(timeConstantMs, sampleRate);
}

void LogParameterSmoother::reset(Sample initialValue) {
    current_ = initialValue > 0.0f ? initialValue : 1e-6f;
    target_ = current_;
}

void LogParameterSmoother::setTarget(Sample target) {
    target_ = target > 0.0f ? target : 1e-6f;
}

void LogParameterSmoother::setTimeConstant(Sample timeConstantMs, Sample sampleRate) {
    sampleRate_ = sampleRate;
    Sample tau = timeConstantMs * 0.001f;
    coeff_ = std::exp(-1.0f / (tau * sampleRate));
    coeff_ = clamp(coeff_, 0.0f, 0.99999f);
}

Sample LogParameterSmoother::process() {
    // Smooth in log domain
    Sample logCurrent = std::log(current_);
    Sample logTarget = std::log(target_);
    logCurrent = logTarget + coeff_ * (logCurrent - logTarget);
    current_ = std::exp(logCurrent);
    return current_;
}

void LogParameterSmoother::processBlock(Sample* buffer, size_t numFrames, Sample target) {
    setTarget(target);
    for (size_t i = 0; i < numFrames; ++i) {
        buffer[i] = process();
    }
}

// AttackReleaseSmoother implementation
AttackReleaseSmoother::AttackReleaseSmoother(Sample attackMs, Sample releaseMs, Sample sampleRate) {
    setTimes(attackMs, releaseMs, sampleRate);
}

void AttackReleaseSmoother::reset(Sample initialValue) {
    current_ = initialValue;
}

void AttackReleaseSmoother::setTimes(Sample attackMs, Sample releaseMs, Sample sampleRate) {
    Sample tauA = attackMs * 0.001f;
    Sample tauR = releaseMs * 0.001f;
    attackCoeff_ = std::exp(-1.0f / (tauA * sampleRate));
    releaseCoeff_ = std::exp(-1.0f / (tauR * sampleRate));
    attackCoeff_ = clamp(attackCoeff_, 0.0f, 0.99999f);
    releaseCoeff_ = clamp(releaseCoeff_, 0.0f, 0.99999f);
}

Sample AttackReleaseSmoother::process(Sample input) {
    if (input > current_) {
        current_ = input + attackCoeff_ * (current_ - input);
    } else {
        current_ = input + releaseCoeff_ * (current_ - input);
    }
    return current_;
}

void AttackReleaseSmoother::processBlock(const Sample* input, Sample* output, size_t numFrames) {
    for (size_t i = 0; i < numFrames; ++i) {
        output[i] = process(input[i]);
    }
}

} // namespace dsp
} // namespace lxx