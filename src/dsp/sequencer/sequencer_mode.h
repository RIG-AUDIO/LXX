#pragma once

#include <array>
#include <cstdint>

#include "engines/drum_events.h"
#include "sequencer/sequencer_types.h"

namespace lxx {
namespace dsp {

struct SequencerModeParams {
    SequencerModeId mode = SequencerModeId::legacy;
    bool running = false;
    uint8_t tempo = 120;
    bool hostSync = true;
    uint8_t clockDivision = 4;
    uint8_t swing = 0; // 0..127 legacy

    // Shared topographic / legacy map
    uint8_t drumsX = 128;
    uint8_t drumsY = 128;
    std::array<uint8_t, 3> density { 128, 128, 128 };

    // Grids chaos
    uint8_t gridsChaos = 0;

    // Techno macros 0..1
    float technoDensity = 0.45f;
    float technoVariation = 0.25f;
    float technoSyncopation = 0.2f;
    float technoFill = 0.35f;
    float technoSwing = 0.0f;
    uint32_t technoSeed = 1;
};

class ISequencerMode {
public:
    virtual ~ISequencerMode() = default;
    virtual void prepare(double sampleRate) = 0;
    virtual void reset() = 0;
    virtual void setParams(const SequencerModeParams& params) = 0;
    virtual void setRunning(bool running) = 0;
    virtual uint32_t process(uint32_t numSamples,
                             const HostClockInput& host,
                             DrumEventBuffer& out) = 0;
    virtual uint8_t grooveStep() const = 0;
    virtual SequencerModeId id() const = 0;
};

}  // namespace dsp
}  // namespace lxx
