#include "adsr_envelope.h"
#include <cmath>

namespace lxx {
namespace dsp {

// AdsrEnvelope implementation
AdsrEnvelope::AdsrEnvelope(Sample sampleRate) : sampleRate_(sampleRate), curves_() {
    init(sampleRate);
}

void AdsrEnvelope::init(Sample sampleRate) {
    sampleRate_ = sampleRate;
    reset();
    // Default times (will be overwritten by setTimes/setFromParams)
    setTimes(0.001f, 0.1f, 0.5f, 0.2f);
}

void AdsrEnvelope::reset() {
    segment_ = static_cast<uint8_t>(EnvSegment::DEAD);
    phase_ = 0.0;
    value_ = 0.0f;
    segStart_ = 0.0f;
    segEnd_ = 0.0f;
    wasActive_ = false;
}

void AdsrEnvelope::setTimes(Sample attack, Sample decay, Sample sustain, Sample release) {
    attackTime_ = std::max(attack, 0.0001f);      // Min 0.1ms
    decayTime_ = std::max(decay, 0.0001f);
    sustainLevel_ = clamp(sustain, 0.0f, 1.0f);
    releaseTime_ = std::max(release, 0.0001f);
    
    updateIncrements();
}

void AdsrEnvelope::setFromParams(uint8_t attack, uint8_t decay, uint8_t sustain, uint8_t release) {
    // Generate gamma-law increments (matches lookup_tables.py)
    auto increments = EnvelopeCurves::generateEnvIncrements();
    
    // Map 0-255 parameter to increment
    // The original uses: increment = pgm_read_dword(lut_res_env_increments + param)
    // Where lut_res_env_increments is the gamma-law table
    
    attackInc_ = increments[attack];
    decayInc_ = increments[decay];
    releaseInc_ = increments[release];
    
    // Sustain is linear 0-255 -> 0-1
    sustainLevel_ = sustain / 255.0f;
    
    // Update targets
    target_[static_cast<size_t>(EnvSegment::ATTACK)] = 1.0f;
    target_[static_cast<size_t>(EnvSegment::DECAY)] = sustainLevel_;
    target_[static_cast<size_t>(EnvSegment::SUSTAIN)] = sustainLevel_;
    target_[static_cast<size_t>(EnvSegment::RELEASE)] = 0.0f;
    target_[static_cast<size_t>(EnvSegment::DEAD)] = 0.0f;
    
    // Convert 32.32 fixed-point increment to per-sample phase increment
    // Original: phase_ += increment_[segment] (32.32)
    // phase_ is 32-bit, increment is 32.32 (16.16? Actually 32.32 from pgm_read_dword)
    // Wait: lookup_tables.py generates 32-bit values for env_increments
    // And envelope.h uses uint32_t increment_[ENV_NUM_SEGMENTS]
    // phase_ is uint32_t, phase_ += increment
    // if (phase_ < increment) -> segment done
    // value_ = Mix(a, b, InterpolateIncreasing(lut_res_env_expo, phase_ >> 16))
    // So phase_ is 32-bit, upper 16 bits used for table lookup
    
    // Convert to per-sample double increment
    // At control rate: 20MHz / 510 / 8 / 2 = ~2441 Hz
    // But we run at audio rate (48kHz)
    // So we need to scale: increment_audio = increment_control * (control_rate / audio_rate)
    
    constexpr Sample controlRate = 20000000.0f / 510.0f / 8.0f / 2.0f;
    Sample scale = controlRate / sampleRate_;
    
    attackInc_ *= scale;
    decayInc_ *= scale;
    releaseInc_ *= scale;
}

void AdsrEnvelope::updateIncrements() {
    // Convert time to phase increment
    // Phase goes from 0 to 2^32-1 over the segment time
    // increment = 2^32 / (time * sampleRate)
    constexpr double kPhaseMax = 4294967296.0; // 2^32
    
    attackInc_ = kPhaseMax / (attackTime_ * sampleRate_);
    decayInc_ = kPhaseMax / (decayTime_ * sampleRate_);
    releaseInc_ = kPhaseMax / (releaseTime_ * sampleRate_);
    
    target_[static_cast<size_t>(EnvSegment::ATTACK)] = 1.0f;
    target_[static_cast<size_t>(EnvSegment::DECAY)] = sustainLevel_;
    target_[static_cast<size_t>(EnvSegment::SUSTAIN)] = sustainLevel_;
    target_[static_cast<size_t>(EnvSegment::RELEASE)] = 0.0f;
    target_[static_cast<size_t>(EnvSegment::DEAD)] = 0.0f;
}

void AdsrEnvelope::trigger(EnvSegment segment) {
    uint8_t seg = static_cast<uint8_t>(segment);
    
    if (segment == EnvSegment::DEAD || segment == EnvSegment::ATTACK) {
        value_ = 0.0f;
    }
    
    segStart_ = value_;
    segEnd_ = target_[seg];
    segment_ = seg;
    phase_ = 0.0;
    
    // Set phase increment for this segment
    switch (segment) {
        case EnvSegment::ATTACK: phaseInc_ = attackInc_; break;
        case EnvSegment::DECAY: phaseInc_ = decayInc_; break;
        case EnvSegment::RELEASE: phaseInc_ = releaseInc_; break;
        default: phaseInc_ = 0.0; break;
    }
    
    wasActive_ = true;
}

void AdsrEnvelope::noteOn() {
    trigger(EnvSegment::ATTACK);
}

void AdsrEnvelope::noteOff() {
    trigger(EnvSegment::RELEASE);
}

void AdsrEnvelope::kill() {
    trigger(EnvSegment::DEAD);
}

Sample AdsrEnvelope::renderSegment() {
    if (segment_ == static_cast<uint8_t>(EnvSegment::DEAD)) {
        value_ = 0.0f;
        return 0.0f;
    }

    if (phaseInc_ == 0.0) {
        // SUSTAIN - hold value
        return segEnd_;
    }
    
    phase_ += phaseInc_;
    
    // Check if segment complete (phase wrapped)
    if (phase_ >= 4294967296.0) { // 2^32
        phase_ = 4294967295.0; // Clamp
        value_ = segEnd_;
        
        // Auto-advance to next segment
        if (segment_ != static_cast<uint8_t>(EnvSegment::DEAD) &&
            segment_ != static_cast<uint8_t>(EnvSegment::SUSTAIN)) {
            trigger(static_cast<EnvSegment>(segment_ + 1));
        }
        return value_;
    }
    
    // Interpolate using curve
    // Original uses: InterpolateIncreasing(lut_res_env_expo, phase_ >> 16)
    // Upper 16 bits of phase (0-65535) index into 257-point table
    uint32_t phase32 = static_cast<uint32_t>(phase_);
    uint16_t phase16 = static_cast<uint16_t>(phase32 >> 16);
    Sample curveVal = curves_.getValue16(curveType_, phase16);
    
    value_ = lerp(segStart_, segEnd_, curveVal);
    return value_;
}

Sample AdsrEnvelope::process() {
    return renderSegment();
}

void AdsrEnvelope::processBlock(Sample* buffer, size_t numFrames) {
    for (size_t i = 0; i < numFrames; ++i) {
        buffer[i] = process();
    }
}

void AdsrEnvelope::retrigger() {
    // For legato: restart attack from current value
    if (isActive()) {
        segStart_ = value_;
        segEnd_ = 1.0f;
        segment_ = static_cast<uint8_t>(EnvSegment::ATTACK);
        phase_ = 0.0;
        phaseInc_ = attackInc_;
    } else {
        noteOn();
    }
}

// VcaEnvelope implementation
VcaEnvelope::VcaEnvelope(Sample sampleRate) : env_(sampleRate) {
    init(sampleRate);
}

void VcaEnvelope::init(Sample sampleRate) {
    env_.init(sampleRate);
    baseAttack_ = 0.001f;
    baseDecay_ = 0.1f;
    baseSustain_ = 0.5f;
    baseRelease_ = 0.2f;
    morph_ = 0.5f;
    updateTimes();
}

void VcaEnvelope::setMorph(Sample morph) {
    morph_ = clamp(morph, 0.0f, 1.0f);
    updateTimes();
}

void VcaEnvelope::setBaseTimes(Sample attack, Sample decay, Sample sustain, Sample release) {
    baseAttack_ = attack;
    baseDecay_ = decay;
    baseSustain_ = sustain;
    baseRelease_ = release;
    updateTimes();
}

void VcaEnvelope::updateTimes() {
    // Port of voice.cc UpdateEnvelopeParameters morph logic:
    // morph < 128: AD -> ADSR blend
    // morph >= 128: ADSR -> fast AD blend
    
    Sample attack, decay, sustain, release;
    
    if (morph_ < 0.5f) {
        // 0.0 to 0.5: blend AD (attack=0, decay=decay/2, sustain=high, release=fast)
        // with full ADSR
        Sample m = morph_ * 2.0f; // 0 to 1
        attack = lerp(baseAttack_, 0.0f, m);
        decay = lerp(baseDecay_, baseDecay_ * 0.5f, m);
        sustain = lerp(baseSustain_, 192.0f/255.0f, m); // 192/255 ≈ 0.75
        release = lerp(baseRelease_, baseRelease_ * 0.5f, m);
    } else {
        // 0.5 to 1.0: blend fast AD (attack=0, decay->0, sustain=1, release=very fast)
        // with previous
        Sample m = (morph_ - 0.5f) * 2.0f; // 0 to 1
        attack = 0.0f;
        decay = lerp(baseDecay_ * 0.5f, 0.0f, m);
        sustain = lerp(192.0f/255.0f, 1.0f, m);
        release = lerp(baseRelease_ * 0.5f, 8.0f/1000.0f, m); // 8ms
    }
    
    env_.setTimes(attack, decay, sustain, release);
}

void VcaEnvelope::noteOn() { env_.noteOn(); }
void VcaEnvelope::noteOff() { env_.noteOff(); }
void VcaEnvelope::kill() { env_.kill(); }

Sample VcaEnvelope::process() { return env_.process(); }
void VcaEnvelope::processBlock(Sample* buffer, size_t numFrames) { env_.processBlock(buffer, numFrames); }

// ModEnvelope implementation
ModEnvelope::ModEnvelope(Sample sampleRate) : env_(sampleRate) {
    init(sampleRate);
}

void ModEnvelope::init(Sample sampleRate) {
    env_.init(sampleRate);
    // Mod envelope is AD only (no sustain, release = decay)
    env_.setTimes(0.001f, 0.1f, 0.0f, 0.1f);
}

void ModEnvelope::setTimes(Sample attack, Sample decay) {
    env_.setTimes(attack, decay, 0.0f, decay);
}

void ModEnvelope::noteOn() { env_.noteOn(); }
void ModEnvelope::noteOff() { env_.noteOff(); }
void ModEnvelope::kill() { env_.kill(); }

Sample ModEnvelope::process() { return env_.process(); }
void ModEnvelope::processBlock(Sample* buffer, size_t numFrames) { env_.processBlock(buffer, numFrames); }

} // namespace dsp
} // namespace lxx