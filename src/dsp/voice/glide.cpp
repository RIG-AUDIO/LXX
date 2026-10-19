#include "glide.h"
#include <algorithm>
#include <cmath>

namespace lxx {
namespace dsp {

namespace {
constexpr Sample kFirmwareControlRate = 20000000.0f / 510.0f / 8.0f / 2.0f;
}

void PitchGlide::init(Sample sampleRate) {
    sampleRate_ = sampleRate;
    increments_ = EnvelopeCurves::generateGlideIncrements();
    counter_ = 0;
    incrementPerSample_ = 0;
    source_ = target_ = current_ = 0;
}

void PitchGlide::setGlideTime(Sample timeSec) {
    timeSec = clamp(timeSec, 0.0f, kGlideMaxTime);
    if (timeSec <= 1.0e-4f) {
        incrementPerSample_ = 0;
        return;
    }
    const int index = static_cast<int>(std::round((timeSec / kGlideMaxTime) * 255.0f));
    const uint16_t incControl = increments_[static_cast<size_t>(index)];

    // Scale firmware control-rate increment to audio rate.
    const Sample scale = kFirmwareControlRate / sampleRate_;
    incrementPerSample_ = static_cast<uint32_t>(std::max(incControl * scale, 0.0f));
}

Sample PitchGlide::mixPitch(int16_t a, int16_t b, uint16_t balance) {
    return static_cast<Sample>(a)
         + static_cast<Sample>(b - a) * static_cast<Sample>(balance) / 65536.0f;
}

void PitchGlide::glideTo(int16_t targetPitch) {
    source_ = current_;
    target_ = targetPitch;
    counter_ = 0;
}

void PitchGlide::snapTo(int16_t pitch) {
    source_ = target_ = current_ = pitch;
    counter_ = 0;
    incrementPerSample_ = 0;
}

int16_t PitchGlide::process() {
    if (incrementPerSample_ == 0) {
        current_ = target_;
        return current_;
    }

    const uint32_t prev = counter_;
    counter_ += incrementPerSample_;

    // Unsigned wrap detection (voice.cc: pitch_counter_ < pitch_increment_)
    if (counter_ < prev) {
        counter_ = 0;
        incrementPerSample_ = 0;
        current_ = target_;
        return current_;
    }

    const uint16_t balance = static_cast<uint16_t>(counter_ >> 16);
    current_ = static_cast<int16_t>(mixPitch(source_, target_, balance));
    return current_;
}

} // namespace dsp
} // namespace lxx
