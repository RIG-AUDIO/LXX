#pragma once

#include "../core/dsp_types.h"
#include "lfo_shapes.h"

namespace lxx {
namespace dsp {

/**
 * LFO - Full port of anu::Lfo
 * 
 * Features:
 * - 8 waveforms (triangle, square, ramp up/down, S&H, Bernoulli, lines, noise)
 * - Frequency range: ~0.06 Hz to 100 Hz
 * - MIDI clock sync (PLL via setTargetPhase)
 * - Phase reset, shape change
 * - Output: 16-bit (0-65535), centered at 32768 for bipolar use
 */

class Lfo {
public:
    Lfo(Sample sampleRate = kSampleRate);
    
    void init(Sample sampleRate = kSampleRate);
    void reset();
    
    // Set shape (0-7 matching LfoShape enum)
    void setShape(uint8_t shape);
    void setShape(LfoShape shape) { setShape(static_cast<uint8_t>(shape)); }
    
    // Set frequency in Hz
    void setFrequency(Sample freqHz);
    
    // Set phase increment directly (32.32 fixed point style)
    void setPhaseIncrement(uint32_t increment);
    
    // Get current phase increment
    uint32_t getPhaseIncrement() const { return state_.phaseIncrement_; }
    
    // Set absolute phase (0-1)
    void setPhase(Sample phase);
    
    // PLL target phase for MIDI clock sync
    // Port of set_target_phase from lfo.h
    void setTargetPhase(uint16_t targetPhase);
    
    // Process one sample, returns 0-65535
    uint16_t process();
    
    // Process block
    void processBlock(uint16_t* output, size_t numFrames);
    
    // Get bipolar output (-32768 to +32767)
    int16_t processBipolar();
    
    // Get unipolar output (0.0 to 1.0)
    Sample processUnipolar();
    
    // Check if phase wrapped this sample
    bool looped() const { return state_.looped_; }
    
    // Get current phase (0-1)
    Sample getPhase() const { return static_cast<Sample>(state_.phase_ * (1.0 / 4294967296.0)); }
    
    // Set sample rate (recalculates increments)
    void setSampleRate(Sample sampleRate);

private:
    Sample sampleRate_;
    LfoState state_;
    
    // Convert Hz to phase increment
    uint32_t hzToIncrement(Sample freqHz) const;
    
    // MIDI clock sync state (for PLL)
    static constexpr uint32_t kMidiClocksPerQuarter = 24;
};

} // namespace dsp
} // namespace lxx