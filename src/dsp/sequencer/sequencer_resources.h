#pragma once

#include "sequencer/sequencer_types.h"

#include <array>
#include <cstdint>

namespace lxx {
namespace dsp {

// Ported from upstream anu/resources.cc
extern const std::array<int16_t, kGrooveSteps> kGrooveSwing;
extern const std::array<int16_t, kGrooveSteps> kGrooveShuffle;
extern const std::array<uint16_t, 6> kArpeggiatorPatterns;

extern const std::array<uint8_t, 16> kArpSlideProbability;
extern const std::array<uint8_t, 16> kArpAccentLevel;

const int16_t* grooveTemplateTable(uint8_t templateIndex);

// Independent timing reference (derived from clock.cc constants, not used by runtime).
int32_t firmwareTempoFactor();
int32_t firmwareBaseTickDuration(uint8_t bpm);
double independentStepPeriodSeconds(uint8_t bpm);

}  // namespace dsp
}  // namespace lxx
