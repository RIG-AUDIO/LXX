#pragma once

#include "sequencer/sequencer_resources.h"
#include "sequencer/sequencer_types.h"

namespace lxx {
namespace dsp {

// Port of anu::Clock + voice_controller clock division/prescaler chain.
class ClockEngine {
public:
    static constexpr std::array<uint8_t, 3> kClockDivisions { 1, 2, 6 };
    static constexpr std::array<uint8_t, 3> kInternalRateCompensation { 6, 3, 1 };

    void reset();
    void update(uint8_t bpm, uint8_t swing, uint8_t ppqnIndex, uint8_t grooveTemplate = 1);

    // Firmware-identical tick path (39 kHz virtual timer).
    void tickFirmwareTimer();
    uint8_t countEvents();

    // Audio-thread path: accumulate samples at host rate.
    void processSamples(uint32_t numSamples, double sampleRate);

    bool consumeStepPulse();
    uint8_t pendingStepPulses() const { return pendingStepPulses_; }
    uint8_t grooveStep() const { return stepCount_; }

    const std::array<uint16_t, kGrooveSteps>& tickDurationTable() const { return tickDurationTable_; }
    uint16_t currentTickDuration() const { return tickDuration_; }

private:
    void rebuildTickTable(uint8_t bpm, uint8_t swing, uint8_t grooveTemplate);

    std::array<uint16_t, kGrooveSteps> tickDurationTable_ {};
    uint16_t tickDuration_ = 0;
    uint16_t clockCounter_ = 0;
    uint8_t tickCount_ = 0;
    uint8_t stepCount_ = 0;
    uint8_t prescaler_ = 6;
    uint8_t prescalerCounter_ = 0;
    uint8_t pendingClockEvents_ = 0;

    uint8_t ppqnIndex_ = 0;
    uint8_t voiceClockCounter_ = 0;
    bool stepPulsePending_ = false;
    uint8_t pendingStepPulses_ = 0;

    double sampleAccumulator_ = 0.0;
    double samplesPerFirmwareTick_ = 0.0;
};

}  // namespace dsp
}  // namespace lxx
