#pragma once

#include "../core/dsp_types.h"
#include <cstdint>

namespace lxx {
namespace dsp {

/**
 * AS2164 exponential gain law for the Anushri VCA channel.
 *
 * Raw chip law (AS2164 datasheet): gain = exp(-cv / (60 * k * T))
 * VCA amplitude path inverts DAC voltage so dac=0 → silence.
 */

constexpr Sample kAs2164BoltzmannK = 8.617333262e-5f;
constexpr Sample kPotTaperTempKelvin = 303.13f;  // 273.13 + 30 (30 °C)
constexpr Sample kVcaDacMax = 4095.0f;

class As2164Curve {
public:
    /** Raw AS2164 gain from control voltage in volts. */
    static Sample gainFromControlVoltage(Sample cv, Sample temperature = kRoomTempKelvin);

    /** Convert 12-bit DAC code to output voltage (0–5 V). */
    static Sample dacToVolts(Sample dac);

    /** Convert output voltage to 12-bit DAC code. */
    static Sample voltsToDac(Sample volts);

    /**
     * VCA audio gain from 12-bit DAC code.
     * Applies (Vmax - Vdac) before the exponential law for correct amplitude direction.
     */
    static Sample dacToGain(Sample dac, Sample temperature = kRoomTempKelvin);

    /** VCA audio gain from DAC output voltage. */
    static Sample dacVoltsToGain(Sample dacVolts, Sample temperature = kRoomTempKelvin);

    /** Firmware U8Mix — matches upstream avrlib/op.h / voice.cc. */
    static uint8_t mixU8(uint8_t a, uint8_t b, uint8_t mix);

    /** Firmware U16U8MulShift8. */
    static uint16_t mulU16U8Shift8(uint16_t a, uint8_t b);

    /**
     * VCA DAC from voice.cc:
     * vca_cv = (envelope × velocity_mix × volume) >> 4
     */
    static uint16_t computeFirmwareVcaDac(uint16_t envelopeRender, uint8_t velocityMix, uint8_t volume);

    /** Float envelope (0–1), velocity (0–1), vel amount (0–1), volume (0–255). */
    static Sample computeFirmwareVcaDacFloat(Sample envelope,
                                             Sample velocity,
                                             Sample velAmount,
                                             Sample volume255);
};

} // namespace dsp
} // namespace lxx
