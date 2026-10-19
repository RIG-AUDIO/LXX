#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "sequencer/sequencer_types.h"

namespace lxx {
namespace dsp {

class DrumSequencer {
public:
    void start();
    std::vector<DrumHitEvent> clockStep(const SequencerSettings& settings,
                                          bool sequencerRunning,
                                          uint32_t* rngState = nullptr);

    uint8_t currentStep() const { return step_; }

private:
    static uint8_t readDrumMap(uint8_t step, uint8_t instrument, uint8_t x, uint8_t y);
    static uint8_t u8Mix(uint8_t a, uint8_t b, uint8_t mix);
    static uint8_t u8ShiftLeft4(uint8_t value) { return static_cast<uint8_t>(value << 4); }

    uint8_t step_ = 0;
    std::array<uint8_t, kNumDrumParts> perturbation_ {};
};

}  // namespace dsp
}  // namespace lxx
