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

    // Firmware (anu/voice.cc) advances a 16-bit pitch_counter_ by incControl
    // once per control period and uses that counter directly as the 0..65535
    // mix balance; a full sweep therefore takes 65536/incControl control
    // periods, which is what the table's gamma law encodes.
    //
    // counter_ here is the same counter held in 16.16 form (uint32, with the
    // balance taken from the upper 16 bits) so the sweep can advance per audio
    // sample instead of per control period.  The increment must therefore be
    // scaled to counts per sample *and* by 65536:
    //
    //   counts/sample = incControl * (controlRate / sampleRate) * 65536
    //
    // Without the 65536 factor the sweep would take 65536x the firmware time.
    const double scale = static_cast<double>(kFirmwareControlRate)
                       / static_cast<double>(sampleRate_);
    const double perSample = static_cast<double>(incControl) * scale * 65536.0;
    if (!(perSample > 0.0)) {
        incrementPerSample_ = 0;
        return;
    }
    incrementPerSample_ = static_cast<uint32_t>(
        std::min(perSample, 4294967295.0));
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
