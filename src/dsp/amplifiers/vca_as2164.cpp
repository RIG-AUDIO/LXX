#include "vca_as2164.h"
#include <cmath>

namespace lxx {
namespace dsp {

void VcaAs2164::setTemperature(Sample tempKelvin) {
    temperature_ = tempKelvin;
    updateGainFromDac();
}

void VcaAs2164::setSoftClipEnabled(bool enabled) {
    softClip_ = enabled;
}

void VcaAs2164::setDac(Sample dac) {
    dac_ = clamp(dac, 0.0f, kVcaDacMax);
    updateGainFromDac();
}

void VcaAs2164::setControlVoltage(Sample cvVolts) {
    gain_ = As2164Curve::gainFromControlVoltage(cvVolts, temperature_);
}

void VcaAs2164::setFromFirmware(Sample envelope,
                                 Sample velocity,
                                 Sample velAmount,
                                 Sample volume255) {
    setDac(As2164Curve::computeFirmwareVcaDacFloat(envelope, velocity, velAmount, volume255));
}

void VcaAs2164::updateGainFromDac() {
    gain_ = As2164Curve::dacToGain(dac_, temperature_);
}

Sample VcaAs2164::applySoftClip(Sample x) {
    const Sample t = kOtaSaturationThreshold;
    if (x > t) {
        return t + (1.0f - t) * std::tanh((x - t) / (1.0f - t));
    }
    if (x < -t) {
        return -t + (1.0f - t) * std::tanh((x + t) / (1.0f - t));
    }
    return x;
}

Sample VcaAs2164::processSample(Sample input) const {
    if (gain_ == 0.0f)
        return 0.0f;
    Sample out = input * gain_;
    if (softClip_) {
        out = applySoftClip(out);
    }
    return out;
}

void VcaAs2164::processBlock(const Sample* input, Sample* output, size_t numFrames) const {
    for (size_t i = 0; i < numFrames; ++i) {
        output[i] = processSample(input[i]);
    }
}

} // namespace dsp
} // namespace lxx
