#pragma once

#include <cmath>
#include <cstdint>

namespace lxx {
namespace dsp {

/**
 * Soft reconstruction / anti-image filter after the 8-bit ZOH drum path.
 *
 * Hardware Anushri converts drums via ~39 kHz PWM then an analog RC stage
 * (build docs: probe 16 = drums PWM carrier). Firmware intentionally leaves
 * 8-bit amplitude quantization (manual: "quite lo-fi"), but ultrasonics from
 * staircasing are removed by the analog path — not present in a raw float VST.
 *
 * Two cascaded one-poles @ ~12 kHz (rate-aware) ≈ 2nd-order RC reconstruction.
 * Not a "warmth" EQ and not a substitute for fixing envelope/leak bugs.
 */
class DrumReconstructionFilter {
public:
    void prepare(float sampleRate) noexcept
    {
        sampleRate_ = sampleRate > 1.0f && std::isfinite(sampleRate) ? sampleRate : 48000.0f;
        constexpr float kCutoffHz = 10000.0f;
        const float a = std::exp(-2.0f * 3.14159265358979323846f * kCutoffHz / sampleRate_);
        coeff_ = a;
        reset();
    }

    void reset() noexcept
    {
        z1_ = 0.0f;
        z2_ = 0.0f;
    }

    float process(float x) noexcept
    {
        // y = (1-a)*x + a*y_prev  (two stages)
        const float g = 1.0f - coeff_;
        z1_ = g * x + coeff_ * z1_;
        z2_ = g * z1_ + coeff_ * z2_;
        if (!std::isfinite(z2_))
            z2_ = 0.0f;
        return z2_;
    }

    void processBlock(float* io, uint32_t n) noexcept
    {
        for (uint32_t i = 0; i < n; ++i)
            io[i] = process(io[i]);
    }

private:
    float sampleRate_ = 48000.0f;
    float coeff_ = 0.0f;
    float z1_ = 0.0f;
    float z2_ = 0.0f;
};

}  // namespace dsp
}  // namespace lxx
