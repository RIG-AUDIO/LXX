#pragma once

#include "sequencer/generator_common.h"
#include "sequencer/sequencer_mode.h"

#include <cmath>

namespace lxx {
namespace dsp {

/** Drum & Bass / jungle-influenced 3-lane generator (kick/snare/CH). */
class DnbSequencerMode final : public ISequencerMode {
public:
    void prepare(double sampleRate) override
    {
        sampleRate_ = sampleRate > 1.0 ? sampleRate : 48000.0;
        reset();
    }

    void reset() override
    {
        lastStepIndex_ = -1;
        wasPlaying_ = false;
        grooveStep_ = 0;
        internalPpq_ = 0.0;
    }

    void setParams(const SequencerModeParams& params) override { params_ = params; }

    void setRunning(bool running) override
    {
        if (running && !running_)
            reset();
        running_ = running;
    }

    uint32_t process(uint32_t numSamples, const HostClockInput& host, DrumEventBuffer& out) override
    {
        if (!running_ || numSamples == 0)
            return 0;

        const bool playing = !host.enabled || host.isPlaying;
        if (!playing) {
            wasPlaying_ = false;
            lastStepIndex_ = -1;
            return 0;
        }

        const double sr = host.sampleRate > 1.0 ? host.sampleRate : sampleRate_;
        const double bpm = host.bpm > 1.0 ? host.bpm : static_cast<double>(params_.tempo);
        constexpr double kPpqPerStep = 0.25;
        const double ppqPerSample = bpm / (60.0 * sr);
        double blockStartPpq = (host.enabled && host.hasPpq) ? host.ppq : internalPpq_;
        if (!(host.enabled && host.hasPpq))
            internalPpq_ += ppqPerSample * static_cast<double>(numSamples);

        if (!wasPlaying_) {
            lastStepIndex_ = static_cast<int64_t>(std::floor(blockStartPpq / kPpqPerStep)) - 1;
            wasPlaying_ = true;
        }

        const double blockEndPpq = blockStartPpq + ppqPerSample * static_cast<double>(numSamples);
        int64_t step = lastStepIndex_ + 1;
        const int64_t endStep = static_cast<int64_t>(std::floor((blockEndPpq - 1.0e-12) / kPpqPerStep));
        uint32_t produced = 0;
        for (; step <= endStep; ++step) {
            double stepPpq = static_cast<double>(step) * kPpqPerStep;
            if ((step & 1) && params_.technoSwing > 0.0f)
                stepPpq += kPpqPerStep * static_cast<double>(params_.technoSwing) * 0.4;
            int offset = static_cast<int>(std::llround((stepPpq - blockStartPpq) / ppqPerSample));
            if (offset < 0) offset = 0;
            if (offset >= static_cast<int>(numSamples))
                continue;
            evaluateStep(step, static_cast<uint32_t>(offset), out);
            grooveStep_ = static_cast<uint8_t>(step & 15);
            lastStepIndex_ = step;
            ++produced;
        }
        return produced;
    }

    uint8_t grooveStep() const override { return grooveStep_; }
    SequencerModeId id() const override { return SequencerModeId::dnb; }

private:
    float unit(uint32_t step, uint32_t salt) const noexcept
    {
        return genUnit(step, salt, params_.technoSeed + 0xD4B00D00u);
    }

    void evaluateStep(int64_t stepIndex, uint32_t offset, DrumEventBuffer& out)
    {
        const auto ph = makePhrase4(stepIndex);
        const int s = ph.stepInBar;
        const float dens = params_.technoDensity;
        const float var = params_.technoVariation;
        const float sync = params_.technoSyncopation;
        const float fill = params_.technoFill;
        // 2-bar pattern family from seed (identity across bars)
        const uint32_t family = genHash(static_cast<uint32_t>(stepIndex / 32), params_.technoSeed) & 3u;

        // --- KICK: beat-1 gravity + syncopated DNB roles (NOT 4/4) ---
        bool kick = (s == 0);
        float kickVel = 1.0f;
        if (kick && unit(static_cast<uint32_t>(stepIndex), 11) < var * 0.05f && ph.barInPhrase != 0)
            kick = false;

        // Curated syncopation masks by family
        auto kickChance = [&](int step) -> float {
            float c = 0.0f;
            if (step == 3 || step == 6 || step == 10 || step == 11)
                c = 0.35f + dens * 0.35f + sync * 0.2f;
            if (step == 2 || step == 5 || step == 9)
                c = dens * 0.25f + sync * 0.25f;
            if (step == 14 || step == 15)
                c = dens * 0.2f + fill * (ph.phraseEnd ? 0.35f : 0.1f);
            if (step == 7 && family == 1)
                c = 0.4f + dens * 0.3f; // double-kick feel
            if (step == 4 || step == 8 || step == 12)
                c = dens * 0.08f; // rare on techno-quarters
            return c;
        };
        if (!kick) {
            const float c = kickChance(s);
            if (unit(static_cast<uint32_t>(stepIndex), 21) < c) {
                kick = true;
                kickVel = (s == 0) ? 1.0f : 0.7f;
            }
        }
        // Space before/after snare (step 8): reduce kick on 7 and 9 slightly unless sync high
        if (kick && (s == 7 || s == 9) && sync < 0.55f
            && unit(static_cast<uint32_t>(stepIndex), 22) < 0.45f)
            kick = false;

        if (kick)
            emitEvent(out, DrumVoice::kick, offset, kickVel, s == 0);

        // --- SNARE: half-time backbeat on step 8 (beat 3) ---
        bool snare = (s == 8);
        float snareVel = 0.95f;
        if (snare && unit(static_cast<uint32_t>(stepIndex), 31) < var * 0.06f)
            snare = false;
        if (!snare) {
            float chance = 0.0f;
            if (s == 7 || s == 6) // anticipation
                chance = dens * 0.12f + sync * 0.25f;
            if (s == 9 || s == 10) // ghost after
                chance = dens * 0.1f + sync * 0.15f;
            if (s == 12 && family >= 2)
                chance = dens * 0.15f; // secondary
            if (ph.phraseEnd && (s == 13 || s == 14 || s == 15))
                chance += fill * 0.4f;
            if (unit(static_cast<uint32_t>(stepIndex), 41) < chance) {
                snare = true;
                snareVel = (s == 8) ? 0.95f : (ph.phraseEnd ? 0.8f : 0.4f);
            }
        }
        if (snare)
            emitEvent(out, DrumVoice::snare, offset, snareVel, s == 8);

        // --- HAT: 8ths/16ths/broken ---
        bool hat = false;
        float hatVel = 0.5f;
        bool hatAccent = false;
        const bool off8 = (s == 2 || s == 6 || s == 10 || s == 14);
        const bool on8 = (s == 0 || s == 4 || s == 8 || s == 12);
        if (off8) {
            hat = unit(static_cast<uint32_t>(stepIndex), 51) > var * 0.08f;
            hatVel = 0.7f;
            hatAccent = true;
        } else if (on8) {
            hat = unit(static_cast<uint32_t>(stepIndex), 61) < dens * 0.55f;
            hatVel = 0.4f;
        } else {
            // broken 16ths
            hat = unit(static_cast<uint32_t>(stepIndex), 71) < dens * 0.65f + sync * 0.15f;
            hatVel = 0.3f + 0.25f * unit(static_cast<uint32_t>(stepIndex), 81);
            // Controlled gaps
            if ((s == 3 || s == 11) && unit(static_cast<uint32_t>(stepIndex), 82) < 0.35f)
                hat = false;
        }
        if (ph.phraseEnd && !hat && (s & 1)
            && unit(static_cast<uint32_t>(stepIndex), 91) < fill * 0.35f) {
            hat = true;
            hatVel = 0.55f;
        }
        if (hat)
            emitEvent(out, DrumVoice::hat, offset, hatVel, hatAccent);
    }

    SequencerModeParams params_ {};
    double sampleRate_ = 48000.0;
    double internalPpq_ = 0.0;
    bool running_ = false;
    bool wasPlaying_ = false;
    int64_t lastStepIndex_ = -1;
    uint8_t grooveStep_ = 0;
};

}  // namespace dsp
}  // namespace lxx
