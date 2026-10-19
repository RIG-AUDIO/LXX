#pragma once

#include "../core/dsp_types.h"
#include "vca_curve.h"

namespace lxx {
namespace dsp {

/**
 * Anushri VCA — AS2164 exponential gain (60 mV/decade law).
 *
 * Signal flow: envelope × velocity × volume → 12-bit DAC → gain → audio.
 * Optional soft clip is extended-mode only (not default authentic behavior).
 */

class VcaAs2164 {
public:
    VcaAs2164() = default;

    void setTemperature(Sample tempKelvin);
    void setSoftClipEnabled(bool enabled);

    /** Set 12-bit VCA DAC code (0–4095), matching firmware vca_cv. */
    void setDac(Sample dac);

    /** Set chip control voltage directly (volts at exponential input). */
    void setControlVoltage(Sample cvVolts);

    /** Firmware path: linear envelope × velocity × volume → DAC → gain. */
    void setFromFirmware(Sample envelope,
                           Sample velocity,
                           Sample velAmount,
                           Sample volume255 = 240.0f);

    Sample getGain() const { return gain_; }
    Sample getDac() const { return dac_; }

    Sample processSample(Sample input) const;
    void processBlock(const Sample* input, Sample* output, size_t numFrames) const;

private:
    Sample temperature_ = kRoomTempKelvin;
    Sample dac_ = 0.0f;
    Sample gain_ = 0.0f;
    bool softClip_ = false;

    void updateGainFromDac();
    static Sample applySoftClip(Sample x);
};

} // namespace dsp
} // namespace lxx
