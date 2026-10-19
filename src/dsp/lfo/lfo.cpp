#include "lfo.h"
#include <cmath>

namespace lxx {
namespace dsp {

Lfo::Lfo(Sample sampleRate) : sampleRate_(sampleRate) {
    init(sampleRate);
}

void Lfo::init(Sample sampleRate) {
    sampleRate_ = sampleRate;
    reset();
}

void Lfo::reset() {
    state_.phase_ = 0;
    // Don't reset phaseIncrement_ (frequency) - only phase
    state_.previousPhase_ = 0;
    state_.value_ = 0;
    state_.nextValue_ = 0;
    state_.filteredValue_ = 0;
    state_.looped_ = false;
}

void Lfo::setShape(uint8_t shape) {
    state_.shape_ = shape & 0x7; // 0-7
}

void Lfo::setFrequency(Sample freqHz) {
    freqHz = clamp(freqHz, 0.0001f, kLfoMaxFreq);
    state_.phaseIncrement_ = hzToIncrement(freqHz);
}

void Lfo::setPhaseIncrement(uint32_t increment) {
    state_.phaseIncrement_ = increment;
}

void Lfo::setPhase(Sample phase) {
    phase = clamp(phase, 0.0f, 1.0f);
    state_.looped_ = (phase * 4294967296.0) < state_.phase_;
    state_.phase_ = static_cast<uint32_t>(phase * 4294967296.0);
}

void Lfo::setTargetPhase(uint16_t targetPhase) {
    // Port of anu::Lfo::set_target_phase from lfo.h
    // Simple P.D. corrector to track MIDI clock phase/frequency
    
    uint16_t phi = static_cast<uint16_t>(state_.phase_ >> 16);
    int16_t dError = static_cast<int16_t>((65536 / 24) - (phi - state_.previousPhase_));
    int16_t pError = static_cast<int16_t>(targetPhase - phi);
    
    int32_t error = 0;
    error += dError;
    error += pError;
    error <<= 8; // Scale
    
    int64_t newInc = static_cast<int64_t>(state_.phaseIncrement_) + error;
    
    // Tracking range: 15 BPM .. 960 BPM
    // At 48kHz: minInc = 2^32 * 15/60 / 48000 = ~223696, maxInc = 2^32 * 960/60 / 48000 = ~14316557
    // But original uses: 109523 .. 7009509 (at control rate ~2441 Hz)
    // Scale to audio rate: * (48000 / 2441) ≈ * 19.66
    // Min: 109523 * 19.66 ≈ 2.15M, Max: 7009509 * 19.66 ≈ 137.8M
    
    constexpr uint32_t kMinInc = 2150000u;
    constexpr uint32_t kMaxInc = 138000000u;
    
    if (newInc < static_cast<int64_t>(kMinInc)) newInc = kMinInc;
    else if (newInc > static_cast<int64_t>(kMaxInc)) newInc = kMaxInc;
    
    state_.phaseIncrement_ = static_cast<uint32_t>(newInc);
    state_.previousPhase_ = phi;
}

uint16_t Lfo::process() {
    return LfoShapes::generate(state_.shape_, state_);
}

void Lfo::processBlock(uint16_t* output, size_t numFrames) {
    LfoShapes::generateBlock(state_.shape_, state_, output, numFrames);
}

int16_t Lfo::processBipolar() {
    uint16_t uni = process();
    return static_cast<int16_t>(uni) - 32768;
}

Sample Lfo::processUnipolar() {
    return process() * (1.0f / 65535.0f);
}

void Lfo::setSampleRate(Sample sampleRate) {
    sampleRate_ = sampleRate;
    // Recalculate increment for current frequency
    if (state_.phaseIncrement_ != 0) {
        // Current frequency = increment * sampleRate / 2^32
        Sample freq = static_cast<Sample>(state_.phaseIncrement_ * sampleRate_ / 4294967296.0);
        state_.phaseIncrement_ = hzToIncrement(freq);
    }
}

uint32_t Lfo::hzToIncrement(Sample freqHz) const {
    // phaseIncrement = freqHz * 2^32 / sampleRate
    return static_cast<uint32_t>(freqHz * 4294967296.0 / sampleRate_ + 0.5f);
}

} // namespace dsp
} // namespace lxx