#pragma once

#include "../core/dsp_types.h"
#include "envelope_curves.h"
#include <array>

namespace lxx {
namespace dsp {

/**
 * ADSR Envelope - Full port of anu::Envelope
 * 
 * Original uses 32-bit phase accumulator with 16-bit fractional part.
 * Segments: ATTACK, DECAY, SUSTAIN, RELEASE, DEAD
 * Curve: Exponential (matches lut_res_env_expo)
 * 
 * Key differences from original:
 * - Uses float32 for time/level, double for phase accumulator
 * - Sample rate independent (time in seconds)
 * - Exact curve matching via EnvelopeCurves
 */

enum class EnvSegment : uint8_t {
    ATTACK = 0,
    DECAY = 1,
    SUSTAIN = 2,
    RELEASE = 3,
    DEAD = 4,
    NUM_SEGMENTS
};

class AdsrEnvelope {
public:
    AdsrEnvelope(Sample sampleRate = kSampleRate);
    
    void init(Sample sampleRate = kSampleRate);
    void reset();
    
    // Set segment times (in seconds)
    void setTimes(Sample attack, Sample decay, Sample sustain, Sample release);
    
    // Set from original 8-bit parameter values (0-255)
    // Uses gamma-law mapping from EnvelopeCurves
    void setFromParams(uint8_t attack, uint8_t decay, uint8_t sustain, uint8_t release);
    
    // Trigger segment
    void trigger(EnvSegment segment);
    void noteOn();   // Trigger ATTACK
    void noteOff();  // Trigger RELEASE
    void kill();     // Trigger DEAD
    
    // Process one sample
    Sample process();
    
    // Process block
    void processBlock(Sample* buffer, size_t numFrames);
    
    // Get current state
    EnvSegment getSegment() const { return static_cast<EnvSegment>(segment_); }
    Sample getValue() const { return value_; }
    bool isActive() const { return segment_ != static_cast<uint8_t>(EnvSegment::DEAD); }
    bool isSustain() const { return segment_ == static_cast<uint8_t>(EnvSegment::SUSTAIN); }
    
    // Set curve type
    void setCurveType(EnvCurveType type) { curveType_ = type; }
    
    // Retrigger from current value (for legato)
    void retrigger();

private:
    void updateIncrements();
    Sample renderSegment();
    
    Sample sampleRate_;
    EnvCurveType curveType_ = EnvCurveType::Exponential;
    
    // Segment times (seconds)
    Sample attackTime_ = 0.001f;
    Sample decayTime_ = 0.1f;
    Sample sustainLevel_ = 0.5f;
    Sample releaseTime_ = 0.2f;
    
    // Phase increments per sample (32.32 fixed point concept, but using double)
    double attackInc_ = 0.0;
    double decayInc_ = 0.0;
    double releaseInc_ = 0.0;
    
    // Target values for each segment (0.0 - 1.0)
    Sample target_[static_cast<size_t>(EnvSegment::NUM_SEGMENTS)];
    
    // Current segment
    uint8_t segment_ = static_cast<uint8_t>(EnvSegment::DEAD);
    
    // Phase accumulator (32.32 concept)
    double phase_ = 0.0;
    double phaseInc_ = 0.0;
    
    // Current value
    Sample value_ = 0.0f;
    
    // Segment start/end values for interpolation
    Sample segStart_ = 0.0f;
    Sample segEnd_ = 0.0f;
    
    // Curve lookup
    const EnvelopeCurves curves_;
    
    // Legato/retrigger state
    bool wasActive_ = false;
};

/**
 * VCA Envelope with morph (AD <-> ADSR)
 * Port of voice.cc UpdateEnvelopeParameters morph logic
 */
class VcaEnvelope {
public:
    VcaEnvelope(Sample sampleRate = kSampleRate);
    
    void init(Sample sampleRate = kSampleRate);
    
    // Morph: 0.0 = AD (fast), 1.0 = ADSR (full)
    // Intermediate values interpolate per voice.cc logic
    void setMorph(Sample morph);
    
    void setBaseTimes(Sample attack, Sample decay, Sample sustain, Sample release);
    
    void noteOn();
    void noteOff();
    void kill();
    
    Sample process();
    void processBlock(Sample* buffer, size_t numFrames);
    
    EnvSegment getSegment() const { return env_.getSegment(); }
    Sample getValue() const { return env_.getValue(); }
    bool isActive() const { return env_.isActive(); }
    
    void setCurveType(EnvCurveType type) { env_.setCurveType(type); }

private:
    AdsrEnvelope env_;
    Sample baseAttack_, baseDecay_, baseSustain_, baseRelease_;
    Sample morph_ = 0.5f;
    void updateTimes();
};

/**
 * Modulation Envelope (AD only, used for pitch/PW modulation)
 * Port of voice.cc mod_envelope_
 */
class ModEnvelope {
public:
    ModEnvelope(Sample sampleRate = kSampleRate);
    
    void init(Sample sampleRate = kSampleRate);
    void setTimes(Sample attack, Sample decay); // No sustain/release
    
    void noteOn();
    void noteOff();
    void kill();
    
    Sample process();
    void processBlock(Sample* buffer, size_t numFrames);
    
    bool isActive() const { return env_.isActive(); }
    Sample getValue() const { return env_.getValue(); }
    
    void setCurveType(EnvCurveType type) { env_.setCurveType(type); }

private:
    AdsrEnvelope env_;
};

} // namespace dsp
} // namespace lxx