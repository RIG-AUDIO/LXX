#include "sequencer/sequencer_resources.h"

namespace lxx {
namespace dsp {

const std::array<int16_t, kGrooveSteps> kGrooveSwing {
    127, 127, -127, -127, 127, 127, -127, -127,
    127, 127, -127, -127, 127, 127, -127, -127,
};

const std::array<int16_t, kGrooveSteps> kGrooveShuffle {
    127, -127, 127, -127, 127, -127, 127, -127,
    127, -127, 127, -127, 127, -127, 127, -127,
};

const std::array<uint16_t, 6> kArpeggiatorPatterns {
    21845, 62965, 30583, 21065, 27499, 28527,
};

const std::array<uint8_t, 16> kArpSlideProbability {
    1, 0, 4, 4, 2, 0, 8, 6, 1, 1, 0, 0, 0, 0, 15, 15,
};

const std::array<uint8_t, 16> kArpAccentLevel {
    16, 0, 2, 0, 1, 0, 0, 0, 8, 0, 1, 0, 4, 0, 2, 2,
};

const int16_t* grooveTemplateTable(uint8_t templateIndex)
{
    // Firmware TouchClock uses groove_template = 1 -> shuffle table.
    switch (templateIndex) {
        case 0: return kGrooveSwing.data();
        case 1: return kGrooveShuffle.data();
        default: return kGrooveShuffle.data();
    }
}

int32_t firmwareTempoFactor()
{
    // clock.cc: 4 * 2000000 * 60 / 24 / 51
    return 392156;
}

int32_t firmwareBaseTickDuration(uint8_t bpm)
{
    const int32_t rounding = 2 * static_cast<int32_t>(bpm);
    const int32_t denominator = 4 * static_cast<int32_t>(bpm);
    return (firmwareTempoFactor() + rounding) / denominator;
}

double independentStepPeriodSeconds(uint8_t bpm)
{
    // Closed form from firmware: 6 internal ticks/step, 39 kHz ISR.
    const double tickDuration = static_cast<double>(firmwareBaseTickDuration(bpm));
    return (tickDuration * static_cast<double>(kTicksPerGrooveStep)) / kFirmwareClockHz;
}

}  // namespace dsp
}  // namespace lxx
