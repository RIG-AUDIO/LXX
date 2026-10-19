#include "lfo_shapes.h"
#include <cstdint>

namespace lxx {
namespace dsp {

// LCG matching the upstream avrlib::Random (from upstream random.cc)
// x = x * 1664525 + 1013904223
uint32_t LfoShapes::random32(LfoState& state) {
    state.rngState_ = state.rngState_ * 1664525u + 1013904223u;
    return state.rngState_;
}

uint16_t LfoShapes::random16(LfoState& state) {
    return static_cast<uint16_t>(random32(state) >> 16);
}

uint8_t LfoShapes::random8(LfoState& state) {
    return static_cast<uint8_t>(random32(state) >> 24);
}

uint16_t LfoShapes::generate(uint8_t shape, LfoState& state) {
    uint16_t value = 0;
    
    switch (static_cast<LfoShape>(shape)) {
        case LfoShape::TRIANGLE: {
            // Triangle: 0 to 65535 up, then down
            // Original: (phase_ & 0x80000000) ? phase_ >> 15 : ~(phase_ >> 15)
            if (state.phase_ & 0x80000000u) {
                value = static_cast<uint16_t>(state.phase_ >> 15);
            } else {
                value = ~static_cast<uint16_t>(state.phase_ >> 15);
            }
            break;
        }
        
        case LfoShape::SQUARE: {
            // Square: 0 or 65535
            // Original: (phase_ & 0x80000000) ? 0xffff : 0
            value = (state.phase_ & 0x80000000u) ? 0xFFFFu : 0x0000u;
            break;
        }
        
        case LfoShape::RAMP_UP: {
            // Ramp up: 0 to 65535
            // Original: phase_ >> 16
            value = static_cast<uint16_t>(state.phase_ >> 16);
            break;
        }
        
        case LfoShape::RAMP_DOWN: {
            // Ramp down: 65535 to 0
            // Original: ~(phase_ >> 16)
            value = ~static_cast<uint16_t>(state.phase_ >> 16);
            break;
        }
        
        case LfoShape::SAMPLE_HOLD: {
            // Sample & Hold: random value held until next loop
            // Original: if (looped_) value_ = Random::GetWord(); value = value_;
            if (state.looped_) {
                state.value_ = random16(state);
            }
            value = state.value_;
            break;
        }
        
        case LfoShape::BERNOULLI: {
            // Bernoulli: random 0 or 65535 held
            // Original: if (looped_) value_ = (Random::GetWord() & 1) ? 65535 : 0;
            if (state.looped_) {
                state.value_ = (random16(state) & 1) ? 0xFFFFu : 0x0000u;
            }
            value = state.value_;
            break;
        }
        
        case LfoShape::LINES: {
            // Lines: interpolate between random values
            // Original: if (looped_) { value_ = next_value_; next_value_ = Random::GetWord(); }
            // value = Mix(value_, next_value_, phase_ >> 16)
            if (state.looped_) {
                state.value_ = state.nextValue_;
                state.nextValue_ = random16(state);
            }
            // Linear interpolation between value_ and nextValue_
            uint32_t frac = state.phase_ >> 16;
            value = static_cast<uint16_t>(
                state.value_ + ((static_cast<uint32_t>(state.nextValue_) - state.value_) * frac >> 16)
            );
            break;
        }
        
        case LfoShape::NOISE: {
            // White noise: new random every sample
            value = random16(state);
            break;
        }
        
        default:
            value = 0;
            break;
    }
    
    // Advance phase
    state.phase_ += state.phaseIncrement_;
    
    // Detect loop (phase wrapped)
    state.looped_ = (state.phase_ < state.phaseIncrement_);
    
    return value;
}

void LfoShapes::generateBlock(uint8_t shape, LfoState& state, uint16_t* output, size_t numFrames) {
    for (size_t i = 0; i < numFrames; ++i) {
        output[i] = generate(shape, state);
    }
}

} // namespace dsp
} // namespace lxx