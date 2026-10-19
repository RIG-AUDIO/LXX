#pragma once

#include "../core/dsp_types.h"
#include <cstdint>

namespace lxx {
namespace dsp {

/**
 * LFO Shape enum - matches anu::LfoShape from lfo.h
 */
enum class LfoShape : uint8_t {
    TRIANGLE = 0,
    SQUARE = 1,
    RAMP_UP = 2,
    RAMP_DOWN = 3,
    SAMPLE_HOLD = 4,
    BERNOULLI = 5,
    LINES = 6,
    NOISE = 7,
    NUM_SHAPES
};

/**
 * LFO State structure
 * Mirrors anu::Lfo private members
 */
struct LfoState {
    uint32_t phaseIncrement_ = 0;
    uint32_t phase_ = 0;
    uint16_t previousPhase_ = 0;
    uint16_t value_ = 0;
    uint16_t nextValue_ = 0;
    uint32_t filteredValue_ = 0;
    uint32_t rngState_ = 123456789u;
    uint8_t shape_ = 0;
    bool looped_ = false;
};

/**
 * LFO Shape implementations
 * Ported from anu::Lfo::Render() in lfo.cc
 */
class LfoShapes {
public:
    // Generate one sample for given shape
    static uint16_t generate(uint8_t shape, LfoState& state);
    
    // Process block
    static void generateBlock(uint8_t shape, LfoState& state, uint16_t* output, size_t numFrames);

private:
    // Random number generator (matches the upstream avrlib::Random LCG)
    static uint32_t random32(LfoState& state);
    static uint16_t random16(LfoState& state);
    static uint8_t random8(LfoState& state);
};

} // namespace dsp
} // namespace lxx