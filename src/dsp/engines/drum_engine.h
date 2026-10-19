#pragma once

#include "drums/native_extended_voices.h"
#include "engines/drum_events.h"

namespace lxx {
namespace dsp {

/** Native six-voice kit parameters (Kick/Snare/CH via Legacy + LOW/Mid/OH). */
using ExtendedVoiceParams = NativeExtendedVoices::Params;

struct LegacyEngineParams {
    uint8_t balance = 128;
    uint8_t bandwidth = 255;
    bool extended = false;
    uint8_t tone[3] { 128, 128, 128 };
    uint8_t pitch[3] { 60, 108, 136 };
    uint8_t pitchDecay[3] { 42, 36, 0 };
    uint8_t pitchMod[3] { 130, 50, 0 };
    uint8_t ampDecay[3] { 180, 90, 110 };
    uint8_t crunchOrSnappy[3] { 14, 180, 0 };
};

class IDrumEngine {
public:
    virtual ~IDrumEngine() = default;
    virtual void init(float sampleRate) = 0;
    virtual void reset() = 0;
    virtual void trigger(DrumVoice voice, float velocity01) = 0;
    virtual void setLegacyParams(const LegacyEngineParams&) {}
    virtual void setExtendedParams(const ExtendedVoiceParams&) {}
    virtual void processBlock(float* out, uint32_t numSamples) = 0;
    virtual bool isPlaying() const = 0;
    virtual DrumEngineId id() const = 0;
    virtual uint64_t renderCallCount() const { return 0; }
};

}  // namespace dsp
}  // namespace lxx
