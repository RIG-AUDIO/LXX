#pragma once

#include "core/parameter_smoother.h"
#include "drums/drum_reconstruction_filter.h"
#include "drums/native_extended_voices.h"
#include "engines/legacy_drum_engine.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace lxx {
namespace dsp {

/** Per-voice mixer: dB volume + mute (audio only; MIDI unaffected). */
struct VoiceMixerParams {
    float volDb[6] { 0, 0, 0, 0, 0, 0 }; // kick,snare,hat,low,mid,oh — -60..+6
    bool mute[6] { false, false, false, false, false, false };
};

inline float dbToLinearGain(float db) noexcept
{
    if (!std::isfinite(db)) return 1.0f;
    if (db <= -60.0f) return 0.0f;
    if (db > 6.0f) db = 6.0f;
    return std::pow(10.0f, db * 0.05f);
}

/**
 * Single native LXX drum bus:
 * Kick / Snare / Closed Hat → DrumSynth (LegacyDrumEngine)
 * LOW (Boom/Tom) / Mid Perc / Open Hat → NativeExtendedVoices
 *
 * Per-voice volume/mute applied after synthesis with ~8 ms gain smoothing.
 * CH mute does not disable OH choke on CH trigger.
 */
class DrumEngineBus {
public:
    void init(float sampleRate, uint32_t maxBlock)
    {
        sampleRate_ = sampleRate > 1.0f ? sampleRate : 48000.0f;
        core_.init(sampleRate_);
        extended_.init(sampleRate_);
        recon_.prepare(sampleRate_);
        const uint32_t n = maxBlock > 0 ? maxBlock : 2048;
        scratch_.assign(n, 0.0f);
        // Headroom so that the grow-on-demand path in processBlock() does not
        // reallocate on the audio thread for a host block that is larger than
        // the reported maximum but still within a sane bound.
        scratch_.reserve(std::max<std::size_t>(static_cast<std::size_t>(n) * 2u, 4096u));
        sequencerOnly_ = false;
        bypassedRenders_ = 0;
        for (int i = 0; i < 6; ++i) {
            gainSmooth_[i].setTimeConstant(8.0f, sampleRate_);
            gainSmooth_[i].reset(1.0f);
        }
        applyMixerToEngines();
    }

    void reset()
    {
        core_.reset();
        extended_.reset();
        recon_.reset();
        for (int i = 0; i < 6; ++i)
            gainSmooth_[i].reset(effectiveTarget(i));
        applyMixerToEngines();
    }

    void setSequencerOnly(bool on) noexcept { sequencerOnly_ = on; }
    bool sequencerOnly() const noexcept { return sequencerOnly_; }
    uint64_t bypassedRenderCount() const noexcept { return bypassedRenders_; }

    void requestEngine(DrumEngineId /*id*/) noexcept {}
    DrumEngineId activeEngine() const noexcept { return DrumEngineId::native; }

    void setLegacyParams(const LegacyEngineParams& p) { core_.setLegacyParams(p); }
    void setExtendedParams(const ExtendedVoiceParams& p) { extended_.setParams(p); }

    void setMixerParams(const VoiceMixerParams& p)
    {
        mixer_ = p;
        for (int i = 0; i < 6; ++i)
            gainSmooth_[i].setTarget(effectiveTarget(i));
    }

    const VoiceMixerParams& mixerParams() const noexcept { return mixer_; }

    void trigger(DrumVoice voice, float velocity01)
    {
        trigger(voice, velocity01, LowArticulation::boom);
    }

    void trigger(DrumVoice voice, float velocity01, LowArticulation lowArt)
    {
        if (sequencerOnly_)
            return;

        switch (voice) {
            case DrumVoice::kick:
            case DrumVoice::snare:
                core_.trigger(voice, velocity01);
                break;
            case DrumVoice::hat:
                // Mute affects audibility only — still choke OH articulation.
                core_.trigger(voice, velocity01);
                extended_.chokeOpenHat();
                break;
            case DrumVoice::lowTom:
                extended_.triggerLow(velocity01, resolveLowArt(lowArt));
                break;
            case DrumVoice::midPerc:
                extended_.triggerMid(velocity01);
                break;
            case DrumVoice::openHat:
                extended_.triggerOpenHat(velocity01);
                break;
            case DrumVoice::count:
                break;  // sentinel, never a real voice
        }
    }

    void processBlock(float* out, uint32_t numSamples)
    {
        if (numSamples == 0 || out == nullptr)
            return;

        if (sequencerOnly_) {
            ++bypassedRenders_;
            std::fill_n(out, numSamples, 0.0f);
            return;
        }

        // Grow-on-demand last resort: init() reserves headroom, so this only
        // runs for a host block far larger than any realistic maximum.
        if (scratch_.size() < numSamples)
            scratch_.assign(numSamples, 0.0f);

        // Advance smoothers; push current gains into engines once per block.
        for (int i = 0; i < 6; ++i) {
            gainSmooth_[i].setTarget(effectiveTarget(i));
            for (uint32_t s = 0; s < numSamples; ++s)
                gainSmooth_[i].process();
        }
        applyMixerToEngines();

        core_.processBlock(scratch_.data(), numSamples);
        for (uint32_t i = 0; i < numSamples; ++i) {
            float s = scratch_[i];
            if (!std::isfinite(s))
                s = 0.0f;
            out[i] = s;
        }
        extended_.processAdd(out, numSamples);
        // Analog reconstruction after digital ZOH / LUT mix (hardware RC after PWM).
        recon_.processBlock(out, numSamples);
    }

    bool isPlaying() const
    {
        return !sequencerOnly_ && (core_.isPlaying() || extended_.isPlaying());
    }

    LegacyDrumEngine& legacy() noexcept { return core_; }
    NativeExtendedVoices& extended() noexcept { return extended_; }

private:
    float effectiveTarget(int voiceIndex) const noexcept
    {
        if (voiceIndex < 0 || voiceIndex >= 6)
            return 0.0f;
        if (mixer_.mute[voiceIndex])
            return 0.0f;
        return dbToLinearGain(mixer_.volDb[voiceIndex]);
    }

    void applyMixerToEngines()
    {
        core_.synth().setVoiceGains(
            gainSmooth_[0].getCurrent(),
            gainSmooth_[1].getCurrent(),
            gainSmooth_[2].getCurrent());
        extended_.setVoiceGains(
            gainSmooth_[3].getCurrent(),
            gainSmooth_[4].getCurrent(),
            gainSmooth_[5].getCurrent());
    }

    LowArticulation resolveLowArt(LowArticulation suggested) const
    {
        const int mode = extended_.params().lowMode;
        if (mode == 1)
            return LowArticulation::boom;
        if (mode == 2)
            return LowArticulation::tom;
        return suggested;
    }

    float sampleRate_ = 48000.0f;
    LegacyDrumEngine core_;
    NativeExtendedVoices extended_;
    DrumReconstructionFilter recon_;
    VoiceMixerParams mixer_ {};
    ParameterSmoother gainSmooth_[6];
    bool sequencerOnly_ = false;
    uint64_t bypassedRenders_ = 0;
    std::vector<float> scratch_;
};

}  // namespace dsp
}  // namespace lxx
