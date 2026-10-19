#pragma once

#include "sequencer/generator_common.h"
#include "sequencer/techno_sequencer.h"

namespace lxx {
namespace dsp {

// Six-lane orchestration around golden TECHNO core.
class TechnoExtendedSequencerMode final : public ISequencerMode {
public:
    void prepare(double sampleRate) override
    {
        core_.prepare(sampleRate);
        sampleRate_ = sampleRate > 1.0 ? sampleRate : 48000.0;
        reset();
    }

    void reset() override
    {
        core_.reset();
        lastStepIndex_ = -1;
        wasPlaying_ = false;
        grooveStep_ = 0;
        internalPpq_ = 0.0;
        lastKickStep_ = -999;
        rollLeft_ = 0;
    }

    void setParams(const SequencerModeParams& params) override
    {
        params_ = params;
        core_.setParams(params);
    }

    void setRunning(bool running) override
    {
        running_ = running;
        core_.setRunning(running);
        if (running && !wasRunning_)
            reset();
        wasRunning_ = running;
    }

    uint32_t process(uint32_t numSamples, const HostClockInput& host, DrumEventBuffer& out) override
    {
        if (!running_ || numSamples == 0)
            return 0;

        const uint32_t before = out.count;
        core_.process(numSamples, host, out);
        grooveStep_ = core_.grooveStep();

        const bool playing = !host.enabled || host.isPlaying;
        if (!playing) {
            wasPlaying_ = false;
            lastStepIndex_ = -1;
            return out.count - before;
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

        for (; step <= endStep; ++step) {
            double stepPpq = static_cast<double>(step) * kPpqPerStep;
            if ((step & 1) && params_.technoSwing > 0.0f)
                stepPpq += kPpqPerStep * static_cast<double>(params_.technoSwing) * 0.45;
            int offset = static_cast<int>(std::llround((stepPpq - blockStartPpq) / ppqPerSample));
            if (offset < 0) offset = 0;
            if (offset >= static_cast<int>(numSamples))
                continue;

            bool hasKick = hasVoiceAtOffset(out, before, DrumVoice::kick, static_cast<uint32_t>(offset));
            bool hasSnare = hasVoiceAtOffset(out, before, DrumVoice::snare, static_cast<uint32_t>(offset));
            bool hasCH = hasVoiceAtOffset(out, before, DrumVoice::hat, static_cast<uint32_t>(offset));
            if (hasKick)
                lastKickStep_ = step;

            emitExtended(step, static_cast<uint32_t>(offset), hasKick, hasSnare, hasCH, before, out);
            lastStepIndex_ = step;
        }

        enforceHatExclusivityPass(out, before);
        return out.count - before;
    }

    uint8_t grooveStep() const override { return grooveStep_; }
    SequencerModeId id() const override { return SequencerModeId::technoExtended; }

private:
    float unit(uint32_t step, uint32_t salt) const noexcept
    {
        return genUnit(step, salt, params_.technoSeed + 0xE71E2D0u);
    }

    void emitExtended(int64_t stepIndex, uint32_t offset, bool hasKick, bool hasSnare, bool hasCH,
                      uint32_t from, DrumEventBuffer& out)
    {
        const auto ph = makePhrase8(stepIndex);
        const float dens = params_.technoDensity;
        const float sync = params_.technoSyncopation;
        const float fill = params_.technoFill;
        const float var = params_.technoVariation;
        const int s = ph.stepInBar;

        // Density curves: low lane much more present than previous sparse implementation
        const float lowAmt = 0.12f + dens * 0.55f + sync * 0.15f; // medium dens → active
        const float midAmt = dens * 0.5f + sync * 0.3f;
        float ohAmt = dens * 0.32f;
        if (ph.phraseEnd) ohAmt += fill * 0.35f;
        if (ph.phraseEarly) ohAmt *= 0.6f;

        // --- LOW / SUB BOOM: role-based grammar ---
        bool wantLow = false;
        float lowVel = 0.55f;
        bool lowAccent = false;

        // E: continue high-density roll
        if (rollLeft_ > 0 && !hasKick) {
            wantLow = true;
            lowVel = 0.45f;
            --rollLeft_;
        }

        // A: kick response (+2, +3 16ths after last kick)
        if (!wantLow && lastKickStep_ >= 0) {
            const int64_t delta = stepIndex - lastKickStep_;
            if ((delta == 2 || delta == 3) && !hasKick) {
                float chance = lowAmt * (delta == 2 ? 0.55f : 0.4f);
                if (ph.phraseEarly) chance *= 0.7f;
                if (unit(static_cast<uint32_t>(stepIndex), 201) < chance) {
                    wantLow = true;
                    lowVel = 0.5f + 0.2f * unit(static_cast<uint32_t>(stepIndex), 202);
                }
            }
        }

        // B: pre-downbeat pickup
        if (!wantLow && (s == 14 || s == 15 || s == 13) && !hasKick) {
            float chance = lowAmt * 0.35f;
            if (ph.phraseEnd || ph.barEnd)
                chance += fill * 0.35f;
            if (unit(static_cast<uint32_t>(stepIndex), 211) < chance) {
                wantLow = true;
                lowVel = ph.phraseEnd ? 0.85f : 0.6f;
                lowAccent = ph.phraseEnd;
            }
        }

        // C: syncopated low groove in empty kick spaces
        if (!wantLow && !hasKick) {
            float chance = 0.0f;
            if (s == 6 || s == 10)
                chance = lowAmt * 0.4f;
            if (s == 2 || s == 9 || s == 11)
                chance = lowAmt * 0.28f + sync * 0.2f;
            if (s == 5 || s == 7)
                chance = lowAmt * 0.22f;
            if (ph.phraseMid)
                chance *= 1.15f;
            if (unit(static_cast<uint32_t>(stepIndex), 221) < chance) {
                wantLow = true;
                lowVel = 0.45f + 0.25f * unit(static_cast<uint32_t>(stepIndex), 222);
            }
        }

        // D: phrase boom — rare stronger hit
        if (!wantLow && ph.phraseEnd && (s == 12 || s == 14 || s == 0)) {
            if (unit(static_cast<uint32_t>(stepIndex), 231) < fill * 0.4f + lowAmt * 0.15f) {
                wantLow = true;
                lowVel = 0.9f;
                lowAccent = true;
            }
        }

        // Intentional kick+sub at downbeat / peak (low probability)
        if (!wantLow && hasKick && s == 0 && dens > 0.55f
            && unit(static_cast<uint32_t>(stepIndex), 241) < (dens - 0.55f) * 0.25f) {
            wantLow = true;
            lowVel = 0.7f;
            lowAccent = true;
        }

        // E: start short roll at high density
        if (!wantLow && dens > 0.7f && !hasKick && (s == 4 || s == 8)
            && unit(static_cast<uint32_t>(stepIndex), 251) < (dens - 0.7f) * 0.55f) {
            wantLow = true;
            lowVel = 0.5f;
            rollLeft_ = 1 + static_cast<int>(unit(static_cast<uint32_t>(stepIndex), 252) * 2.0f); // 1..2 more
        }

        if (wantLow) {
            // Boom: phrase weight / downbeats / long space; Tom: rolls / syncopation / fills
            LowArticulation art = LowArticulation::boom;
            const bool tomRole = (rollLeft_ > 0)
                || (s == 2 || s == 6 || s == 10 || s == 11)
                || (dens > 0.65f && !lowAccent);
            const bool boomRole = lowAccent || ph.phraseEnd || (hasKick && s == 0)
                || (s == 14 || s == 15);
            if (tomRole && !boomRole)
                art = LowArticulation::tom;
            else if (tomRole && boomRole
                     && unit(static_cast<uint32_t>(stepIndex), 255) > 0.55f)
                art = LowArticulation::tom;
            emitLow(out, offset, lowVel, lowAccent, art);
        }

        // --- MID PERC: avoid snare landings; syncopated answers ---
        if (!hasSnare) {
            float chance = 0.0f;
            if (s == 2 || s == 10)
                chance = midAmt * 0.42f;
            if (s == 7 || s == 9)
                chance = midAmt * 0.38f + sync * 0.22f;
            if (s == 1 || s == 5 || s == 13)
                chance = midAmt * 0.25f;
            if (ph.phraseMid)
                chance *= 1.2f;
            if (ph.phraseEnd && (s & 1))
                chance += fill * 0.22f;
            // Prefer gaps without CH for industrial punctuation
            if (!hasCH)
                chance *= 1.1f;
            if (unit(static_cast<uint32_t>(stepIndex), 261) < chance)
                emitEvent(out, DrumVoice::midPerc, offset,
                          0.4f + 0.3f * unit(static_cast<uint32_t>(stepIndex), 262), false);
        }

        // --- OPEN HAT: replace CH at same step (hard exclusivity) ---
        bool wantOH = false;
        float ohVel = 0.7f;
        if (s == 2 || s == 6 || s == 10 || s == 14) {
            if (unit(static_cast<uint32_t>(stepIndex), 271) < ohAmt * 0.6f)
                wantOH = true;
        }
        if (!wantOH && ph.phraseEnd && s == 14
            && unit(static_cast<uint32_t>(stepIndex), 281) < fill * 0.55f) {
            wantOH = true;
            ohVel = 0.9f;
        }
        if (!wantOH && dens > 0.72f && (s == 1 || s == 9)
            && unit(static_cast<uint32_t>(stepIndex), 291) < (dens - 0.72f) * 0.7f) {
            wantOH = true;
            ohVel = 0.55f;
        }
        if (wantOH && unit(static_cast<uint32_t>(stepIndex), 301) < var * 0.06f)
            wantOH = false;

        if (wantOH) {
            resolveHatExclusivity(out, from, offset, true);
            emitEvent(out, DrumVoice::openHat, offset, ohVel, s == 14);
        }
        (void) hasCH;
    }

    TechnoSequencerMode core_;
    SequencerModeParams params_ {};
    double sampleRate_ = 48000.0;
    double internalPpq_ = 0.0;
    bool running_ = false;
    bool wasRunning_ = false;
    bool wasPlaying_ = false;
    int64_t lastStepIndex_ = -1;
    int64_t lastKickStep_ = -999;
    int rollLeft_ = 0;
    uint8_t grooveStep_ = 0;
};

}  // namespace dsp
}  // namespace lxx
