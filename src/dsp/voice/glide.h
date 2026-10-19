#pragma once

#include "../core/dsp_types.h"
#include "../envelopes/envelope_curves.h"
#include <array>
#include <cstdint>

namespace lxx {
namespace dsp {

/**
 * Portamento — matches upstream anu/voice.cc pitch_counter_/pitch_increment_ glide.
 *
 * Firmware uses lut_res_glide_increments (gamma=0.1, max 6 s) at control rate
 * (~2441 Hz). Increments are scaled to the audio sample rate.
 */
class PitchGlide {
public:
    void init(Sample sampleRate);

    /** Map glide time 0..kGlideMaxTime seconds to firmware table index. */
    void setGlideTime(Sample timeSec);

    /** Begin glide from current pitch to target (1/128 semitone units). */
    void glideTo(int16_t targetPitch);

    /** Snap source and target (no portamento). */
    void snapTo(int16_t pitch);

    int16_t currentPitch() const { return current_; }
    bool isInstant() const { return incrementPerSample_ == 0; }

    /** Advance one sample; returns interpolated pitch. */
    int16_t process();

private:
    static Sample mixPitch(int16_t a, int16_t b, uint16_t balance);

    Sample sampleRate_ = kSampleRate;
    std::array<uint16_t, 256> increments_{};
    uint32_t incrementPerSample_ = 0;

    int16_t source_ = 0;
    int16_t target_ = 0;
    int16_t current_ = 0;
    uint32_t counter_ = 0;
};

} // namespace dsp
} // namespace lxx
