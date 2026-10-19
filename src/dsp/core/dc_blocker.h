#pragma once

#include "dsp_types.h"

#include <cmath>

namespace lxx {
namespace dsp {

/** Host-output DC blocker. Not part of the original firmware DSP. */
class DcBlocker {
public:
    void prepare(double sampleRate) noexcept
    {
        constexpr double kReferenceRate = 44100.0;
        constexpr double kReferencePole = 0.9999;
        const double validRate = sampleRate > 1.0 ? sampleRate : kReferenceRate;
        pole_ = static_cast<Sample>(std::pow(kReferencePole, kReferenceRate / validRate));
        reset();
    }

    void reset() noexcept
    {
        x1_ = 0.0f;
        y1_ = 0.0f;
    }

    Sample process(Sample sample) noexcept
    {
        const Sample y = sample - x1_ + pole_ * y1_;
        x1_ = sample;
        y1_ = y;
        constexpr Sample kFlush = 1.0e-20f;
        if (std::abs(x1_) < kFlush)
            x1_ = 0.0f;
        if (std::abs(y1_) < kFlush)
            y1_ = 0.0f;
        return std::abs(y1_) < kFlush ? 0.0f : y1_;
    }

private:
    Sample pole_ = 0.9999f;
    Sample x1_ = 0.0f;
    Sample y1_ = 0.0f;
};

}  // namespace dsp
}  // namespace lxx
