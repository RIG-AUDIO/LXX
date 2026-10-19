#pragma once

#include "drums/drum_synth.h"
#include "engines/drum_engine.h"

#include <algorithm>

namespace lxx {
namespace dsp {

class LegacyDrumEngine final : public IDrumEngine {
public:
    void init(float sampleRate) override
    {
        sampleRate_ = sampleRate > 1.0f ? sampleRate : 48000.0f;
        synth_.init(sampleRate_);
        renderCalls_ = 0;
    }

    void reset() override { synth_.reset(); }

    void trigger(DrumVoice voice, float velocity01) override
    {
        const int part = legacyPartForVoice(voice);
        if (part < 0)
            return; // extended lanes handled by bus sidechain voices
        float v = velocity01;
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        int level = static_cast<int>(v * 255.0f + 0.5f);
        if (level < 1) level = 1;
        if (level > 255) level = 255;
        synth_.trigger(static_cast<uint8_t>(part), static_cast<uint8_t>(level));
    }

    void setLegacyParams(const LegacyEngineParams& p) override
    {
        synth_.setBalance(p.balance);
        synth_.setBandwidth(p.bandwidth);
        if (p.extended) {
            for (uint8_t i = 0; i < kNumDrumParts; ++i) {
                synth_.setEnginePatch(i, p.pitch[i], p.pitchDecay[i], p.pitchMod[i],
                                      p.ampDecay[i], p.crunchOrSnappy[i]);
            }
        } else {
            for (uint8_t i = 0; i < kNumDrumParts; ++i)
                synth_.morphPatch(i, p.tone[i]);
        }
    }

    void processBlock(float* out, uint32_t numSamples) override
    {
        ++renderCalls_;
        if (out == nullptr || numSamples == 0)
            return;
        if (synth_.isPlaying()) {
            synth_.processBlock(out, numSamples);
        } else {
            synth_.fillWithSilence(numSamples);
            std::fill_n(out, numSamples, 0.0f);
        }
    }

    bool isPlaying() const override { return synth_.isPlaying(); }
    DrumEngineId id() const override { return DrumEngineId::legacy; }
    uint64_t renderCallCount() const override { return renderCalls_; }

    DrumSynth& synth() noexcept { return synth_; }

private:
    DrumSynth synth_;
    float sampleRate_ = 48000.0f;
    uint64_t renderCalls_ = 0;
};

}  // namespace dsp
}  // namespace lxx
