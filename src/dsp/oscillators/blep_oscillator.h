#pragma once

#include "../core/dsp_types.h"
#include <array>

namespace lxx {
namespace dsp {

/**
 * BLEP (Band-Limited Step) Oscillator
 * 
 * Implements anti-aliased triangle, sawtooth, and pulse waveforms
 * using the PolyBLEP method (minimal aliasing, efficient).
 * 
 * For triangle wave (authentic Anushri mode): generate sawtooth and
 * apply triangle conversion, or use direct triangle BLEP.
 * 
 * References:
 * - "Alias-Free Digital Synthesis of Classic Analog Waveforms" (Välimäki & Huovilainen)
 * - PolyBLEP: "Efficient Implementation of Band-Limited Oscillators" (Stilson & Smith)
 */

enum class Waveform : uint8_t {
    TRIANGLE = 0,    // Authentic Anushri VCO waveform
    SAWTOOTH = 1,    // Extended mode
    PULSE = 2,       // Extended mode (variable duty cycle)
    SUB_SQUARE = 3   // Sub-oscillator (1/2 octave square)
};

class BlePOscillator {
public:
    BlePOscillator(Sample sampleRate = kSampleRate);
    
    void setSampleRate(Sample sampleRate);
    void reset();
    
    // Set waveform type
    void setWaveform(Waveform waveform);
    
    // Set frequency in Hz
    void setFrequency(Sample freqHz);
    
    // Set pulse width for PULSE waveform (0.0 to 1.0, 0.5 = square)
    void setPulseWidth(Sample pw);
    
    // Set phase offset (0.0 to 1.0)
    void setPhase(Sample phase);
    
    // Hard sync: reset phase to 0 (for sub-osc sync)
    void hardSync();
    
    // Process one sample
    Sample process();
    
    // Process block
    void processBlock(Sample* output, size_t numFrames);
    
    // Get current phase (0-1)
    Sample getPhase() const { return phase_; }
    
    // Get current waveform
    Waveform getWaveform() const { return waveform_; }
    
    // Generate PolyBLEP correction for a discontinuity
    // t = phase / phase_increment (normalized time through step)
    static Sample polyblep(Sample t);
    
    // Generate MinBLEP correction (higher quality, uses table)
    // Requires pre-computed MinBLEP table
    static Sample minblep(Sample t, const Sample* table, size_t tableSize);

private:
    Sample sampleRate_;
    Sample freqHz_ = 440.0f;
    Waveform waveform_ = Waveform::TRIANGLE;
    Sample phase_ = 0.0f;
    Sample phaseInc_ = 0.0f;
    Sample pulseWidth_ = 0.5f;
    Sample nextSample_ = 0.0f;  // For pulse wave dual-edge BLEP
    Sample integratorState_ = 0.0f;
    Sample subPhase_ = 0.0f;
    Sample subPhaseInc_ = 0.0f;
    
    // MinBLEP table (if used)
    static constexpr size_t kMinBlepTableSize = 4096;
    static const Sample kMinBlepTable[kMinBlepTableSize];
    
    void updatePhaseIncrement();
    
    // Core waveform generation (without BLEP)
    Sample generateRaw(Sample phase) const;
    
    // Apply PolyBLEP to triangle wave
    Sample processTriangle();
    
    // Apply PolyBLEP to sawtooth wave
    Sample processSawtooth();
    
    // Apply PolyBLEP to pulse wave (two edges)
    Sample processPulse();
    
    // Sub-oscillator (square at 1/2 frequency)
    Sample processSubSquare();
};

} // namespace dsp
} // namespace lxx