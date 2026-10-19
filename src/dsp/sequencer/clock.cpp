#include "sequencer/clock.h"

#include <cmath>

namespace lxx {
namespace dsp {

void ClockEngine::reset()
{
    clockCounter_ = 0;
    tickCount_ = 0;
    stepCount_ = 0;
    tickDuration_ = tickDurationTable_[0];
    pendingClockEvents_ = 0;
    prescalerCounter_ = 0;
    voiceClockCounter_ = 0;
    stepPulsePending_ = false;
    pendingStepPulses_ = 0;
    sampleAccumulator_ = 0.0;
}

void ClockEngine::rebuildTickTable(uint8_t bpm, uint8_t swing, uint8_t grooveTemplate)
{
    const int16_t baseTickDuration = static_cast<int16_t>(firmwareBaseTickDuration(bpm));
    const uint8_t grooveAmount = swing >> 1;
    const int16_t* groove = grooveTemplateTable(grooveTemplate);

    for (uint8_t i = 0; i < kGrooveSteps; ++i) {
        int32_t swingDirection = static_cast<int32_t>(groove[i]);
        swingDirection *= baseTickDuration;
        swingDirection *= grooveAmount;
        const int16_t swingOffset = static_cast<int16_t>(swingDirection >> 16);
        tickDurationTable_[i] = static_cast<uint16_t>(baseTickDuration + swingOffset);
    }

    tickDuration_ = tickDurationTable_[0];
}

void ClockEngine::update(uint8_t bpm, uint8_t swing, uint8_t ppqnIndex, uint8_t grooveTemplate)
{
    ppqnIndex_ = static_cast<uint8_t>(ppqnIndex > 2 ? 2 : ppqnIndex);
    prescaler_ = kInternalRateCompensation[ppqnIndex_];
    rebuildTickTable(bpm, swing, grooveTemplate);
}

void ClockEngine::tickFirmwareTimer()
{
    ++clockCounter_;
    if (clockCounter_ >= tickDuration_) {
        ++pendingClockEvents_;
        clockCounter_ = 0;
    }
}

uint8_t ClockEngine::countEvents()
{
    uint8_t count = 0;
    while (pendingClockEvents_ > 0) {
        ++tickCount_;
        if (tickCount_ == kTicksPerGrooveStep) {
            tickCount_ = 0;
            ++stepCount_;
            if (stepCount_ == kGrooveSteps) {
                stepCount_ = 0;
            }
            tickDuration_ = tickDurationTable_[stepCount_];
        }

        ++prescalerCounter_;
        if (prescalerCounter_ >= prescaler_) {
            ++count;
            prescalerCounter_ = 0;
        }

        --pendingClockEvents_;
    }

    const uint8_t division = kClockDivisions[ppqnIndex_];
    for (uint8_t i = 0; i < count; ++i) {
        if (voiceClockCounter_ == 0) {
            ++pendingStepPulses_;
            stepPulsePending_ = true;
        }

        ++voiceClockCounter_;
        if (voiceClockCounter_ >= division) {
            voiceClockCounter_ = 0;
        }
    }

    return count;
}

void ClockEngine::processSamples(uint32_t numSamples, double sampleRate)
{
    if (!(sampleRate > 0.0) || !std::isfinite(sampleRate)) {
        return;
    }

    samplesPerFirmwareTick_ = sampleRate / kFirmwareClockHz;
    if (!(samplesPerFirmwareTick_ > 1.0e-6) || !std::isfinite(samplesPerFirmwareTick_)) {
        samplesPerFirmwareTick_ = 1.0e-6;
    }
    sampleAccumulator_ += static_cast<double>(numSamples);
    if (!std::isfinite(sampleAccumulator_)) {
        sampleAccumulator_ = 0.0;
        return;
    }

    constexpr int kMaxTicksPerCall = 4096;
    int ticks = 0;
    while (sampleAccumulator_ >= samplesPerFirmwareTick_ && ticks < kMaxTicksPerCall) {
        sampleAccumulator_ -= samplesPerFirmwareTick_;
        tickFirmwareTimer();
        countEvents();
        ++ticks;
    }
    if (ticks >= kMaxTicksPerCall) {
        sampleAccumulator_ = std::fmod(sampleAccumulator_, samplesPerFirmwareTick_);
        if (!std::isfinite(sampleAccumulator_) || sampleAccumulator_ < 0.0)
            sampleAccumulator_ = 0.0;
    }
}

bool ClockEngine::consumeStepPulse()
{
    if (pendingStepPulses_ == 0) {
        return false;
    }
    --pendingStepPulses_;
    if (pendingStepPulses_ == 0) {
        stepPulsePending_ = false;
    }
    return true;
}

}  // namespace dsp
}  // namespace lxx
