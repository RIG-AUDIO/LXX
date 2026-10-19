#pragma once

#include "../core/dsp_types.h"
#include "drum_patch.h"

#include <cstddef>
#include <cstdint>

namespace lxx {
namespace dsp {

/**
 * Float32 port of anu::DrumSynth.
 *
 * Preserves firmware phase increments, envelope LUTs, MorphPatch preset
 * interpolation, crunchiness/noise split, and HH sample playback.
 */
class DrumSynth {
public:
    DrumSynth();

    void init(float sampleRate = kSampleRate);
    void reset();
    void seedRandom(uint16_t seed);

    void trigger(uint8_t instrument, uint8_t level);
    void morphPatch(uint8_t instrument, uint8_t value);
    void setEnginePatch(uint8_t instrument,
                        uint8_t pitch,
                        uint8_t pitchDecay,
                        uint8_t pitchMod,
                        uint8_t ampDecay,
                        uint8_t crunchiness);
    static DrumPatch morphedPatch(uint8_t instrument, uint8_t value);
    void setParameterCc(uint8_t cc, uint8_t value);
    void setBalance(uint8_t mix);
    void setBandwidth(uint8_t bandwidth);
    /** Linear amplitude gain per instrument (0=kick,1=snare,2=hat). Default 1. */
    void setVoiceGains(float kick, float snare, float hat);

    void processBlock(float* output, size_t numFrames);
    void fillWithSilence(size_t numFrames);

    bool isPlaying() const { return playing_; }
    uint32_t debugPhaseIncrement(uint8_t instrument) const {
        return instrument < kNumDrumInstruments ? state_[instrument].phase_increment : 0;
    }
    float debugOscillatorHz(uint8_t instrument) const {
        if (instrument >= kNumDrumInstruments)
            return 0.0f;
        return static_cast<float>(
            static_cast<double>(state_[instrument].phase_increment) * sampleRate_
            / 4294967296.0);
    }

private:
    void updateModulations();
    void updateRateScaling();
    void updateBitcrushPeriod();

    static uint8_t u8u8MulShift8(uint8_t a, uint8_t b);
    static int8_t s8u8MulShift8(int8_t a, uint8_t b);
    static uint16_t u8u8Mul(uint8_t a, uint8_t b);
    static uint8_t u8Mix(uint8_t a, uint8_t b, uint8_t balance);
    static uint8_t interpolateSample(const uint8_t* table, uint16_t phase);
    static uint16_t interpolateIncreasing(const uint16_t* table, uint16_t phase);

    DrumPatch patch_[kNumDrumInstruments];
    DrumState state_[kNumDrumInstruments];

    float sampleRate_ = kSampleRate;
    float audioRateScale_ = 1.0f;
    float controlRateScale_ = 1.0f;

    uint8_t sample_ = 128;
    uint32_t sampleCounter_ = 0;
    uint32_t bitcrushPeriod_ = 1;
    uint8_t sampleRateHold_ = 0;
    uint8_t fadeCounter_ = 0;

    uint8_t noiseState_ = 0;
    uint16_t rngState_ = 0x21;

    bool playing_ = false;
    float voiceGain_[kNumDrumInstruments] { 1.0f, 1.0f, 1.0f };
};

}  // namespace dsp
}  // namespace lxx
