#pragma once

#include "dsp_types.h"

namespace lxx {
namespace dsp {

/**
 * 1-pole parameter smoother for log/linear scaling
 * 
 * Used for smoothing parameter changes to avoid zipper noise.
 * Time constant ~10ms (configurable).
 */
class ParameterSmoother {
public:
    ParameterSmoother() = default;
    explicit ParameterSmoother(Sample timeConstantMs, Sample sampleRate = kSampleRate);
    
    void reset(Sample initialValue = 0.0f);
    void setTarget(Sample target);
    void setTimeConstant(Sample timeConstantMs, Sample sampleRate = kSampleRate);
    
    // Process one sample
    Sample process();
    
    // Process block
    void processBlock(Sample* buffer, size_t numFrames, Sample target);
    
    // Get current value without advancing
    Sample getCurrent() const { return current_; }
    
    // Check if settling
    bool isSettled(Sample epsilon = 0.001f) const {
        return std::abs(current_ - target_) < epsilon;
    }

private:
    Sample current_ = 0.0f;
    Sample target_ = 0.0f;
    Sample coeff_ = 0.0f;  // 1 - exp(-1 / (tau * sr))
    
    void updateCoeff();
};

/**
 * Logarithmic parameter smoother
 * For frequency, time, and other log-scaled parameters
 */
class LogParameterSmoother {
public:
    LogParameterSmoother() = default;
    explicit LogParameterSmoother(Sample timeConstantMs, Sample sampleRate = kSampleRate);
    
    void reset(Sample initialValue = 1.0f);
    void setTarget(Sample target);
    void setTimeConstant(Sample timeConstantMs, Sample sampleRate = kSampleRate);
    
    Sample process();
    void processBlock(Sample* buffer, size_t numFrames, Sample target);
    
    Sample getCurrent() const { return current_; }
    bool isSettled(Sample epsilon = 0.001f) const {
        return std::abs(std::log(current_) - std::log(target_)) < epsilon;
    }

private:
    Sample current_ = 1.0f;
    Sample target_ = 1.0f;
    Sample coeff_ = 0.0f;
    Sample sampleRate_ = kSampleRate;
    
    void updateCoeff();
};

/**
 * Smoothed value with separate rise/fall times
 * For envelope followers, peak meters, etc.
 */
class AttackReleaseSmoother {
public:
    AttackReleaseSmoother() = default;
    AttackReleaseSmoother(Sample attackMs, Sample releaseMs, Sample sampleRate = kSampleRate);
    
    void reset(Sample initialValue = 0.0f);
    void setTimes(Sample attackMs, Sample releaseMs, Sample sampleRate = kSampleRate);
    
    Sample process(Sample input);
    void processBlock(const Sample* input, Sample* output, size_t numFrames);
    
    Sample getCurrent() const { return current_; }

private:
    Sample current_ = 0.0f;
    Sample attackCoeff_ = 0.0f;
    Sample releaseCoeff_ = 0.0f;
};

} // namespace dsp
} // namespace lxx