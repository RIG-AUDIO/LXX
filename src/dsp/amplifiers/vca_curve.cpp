#include "vca_curve.h"
#include <cmath>

namespace lxx {
namespace dsp {

Sample As2164Curve::gainFromControlVoltage(Sample cv, Sample temperature) {
    return std::exp(-cv / (kAS2164GainDenom * kAs2164BoltzmannK * temperature));
}

Sample As2164Curve::dacToVolts(Sample dac) {
    return clamp(dac, 0.0f, kVcaDacMax) * (kCVRangeMax / 4096.0f);
}

Sample As2164Curve::voltsToDac(Sample volts) {
    return clamp(volts, kCVRangeMin, kCVRangeMax) * (4096.0f / kCVRangeMax);
}

Sample As2164Curve::dacVoltsToGain(Sample dacVolts, Sample temperature) {
    const Sample cv = kCVRangeMax - clamp(dacVolts, kCVRangeMin, kCVRangeMax);
    return gainFromControlVoltage(cv, temperature);
}

Sample As2164Curve::dacToGain(Sample dac, Sample temperature) {
    // Firmware: vca_cv == 0 is digital silence (anu.cc skips DCO refresh).
    // The raw AS2164 law at cv=5 V still returns ~0.039; that analog floor must
    // not leak oscillators through a closed VCA in the plugin.
    if (dac <= 0.0f)
        return 0.0f;
    return dacVoltsToGain(dacToVolts(dac), temperature);
}

uint8_t As2164Curve::mixU8(uint8_t a, uint8_t b, uint8_t mix) {
    if (b > a) {
        return static_cast<uint8_t>(a + ((static_cast<uint16_t>(b - a) * mix) >> 8));
    }
    return static_cast<uint8_t>(a - ((static_cast<uint16_t>(a - b) * mix) >> 8));
}

uint16_t As2164Curve::mulU16U8Shift8(uint16_t a, uint8_t b) {
    return static_cast<uint16_t>((static_cast<uint32_t>(a) * b) >> 8);
}

uint16_t As2164Curve::computeFirmwareVcaDac(uint16_t envelopeRender, uint8_t velocityMix, uint8_t volume) {
    uint32_t v = static_cast<uint32_t>(mulU16U8Shift8(envelopeRender, velocityMix));
    v = (v * volume) >> 8;
    return static_cast<uint16_t>(v >> 4);
}

Sample As2164Curve::computeFirmwareVcaDacFloat(Sample envelope,
                                               Sample velocity,
                                               Sample velAmount,
                                               Sample volume255) {
    const uint16_t env = static_cast<uint16_t>(clamp(envelope, 0.0f, 1.0f) * 65535.0f + 0.5f);
    const uint8_t vel = static_cast<uint8_t>(clamp(velocity, 0.0f, 1.0f) * 127.0f + 0.5f);
    const uint8_t velMix = mixU8(255, static_cast<uint8_t>(vel << 1),
                                 static_cast<uint8_t>(clamp(velAmount, 0.0f, 1.0f) * 255.0f + 0.5f));
    const uint8_t vol = static_cast<uint8_t>(clamp(volume255, 0.0f, 255.0f) + 0.5f);
    return static_cast<Sample>(computeFirmwareVcaDac(env, velMix, vol));
}

} // namespace dsp
} // namespace lxx
