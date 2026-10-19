#pragma once

#include "sequencer/dnb_sequencer.h"
#include "sequencer/generator_common.h"

namespace lxx {
namespace dsp {

class DnbExtendedSequencerMode final : public ISequencerMode {
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
                stepPpq += kPpqPerStep * static_cast<double>(params_.technoSwing) * 0.4;
            int offset = static_cast<int>(std::llround((stepPpq - blockStartPpq) / ppqPerSample));
            if (offset < 0) offset = 0;
            if (offset >= static_cast<int>(numSamples))
                continue;

            const bool hasKick = hasVoiceAtOffset(out, before, DrumVoice::kick, static_cast<uint32_t>(offset));
            const bool hasSnare = hasVoiceAtOffset(out, before, DrumVoice::snare, static_cast<uint32_t>(offset));
            if (hasKick)
                lastKickStep_ = step;

            emitExtended(step, static_cast<uint32_t>(offset), hasKick, hasSnare, before, out);
            lastStepIndex_ = step;
        }

        enforceHatExclusivityPass(out, before);
        return out.count - before;
    }

    uint8_t grooveStep() const override { return grooveStep_; }
    SequencerModeId id() const override { return SequencerModeId::dnbExtended; }

private:
    float unit(uint32_t step, uint32_t salt) const noexcept
    {
        return genUnit(step, salt, params_.technoSeed + 0xD4BE2700u);
    }

    void emitExtended(int64_t stepIndex, uint32_t offset, bool hasKick, bool hasSnare,
                      uint32_t from, DrumEventBuffer& out)
    {
        const auto ph = makePhrase4(stepIndex);
        const int s = ph.stepInBar;
        const float dens = params_.technoDensity;
        const float sync = params_.technoSyncopation;
        const float fill = params_.technoFill;
        const float var = params_.technoVariation;

        const float lowAmt = 0.15f + dens * 0.6f + sync * 0.2f;
        const float midAmt = dens * 0.45f + sync * 0.35f;
        float ohAmt = dens * 0.22f;
        if (ph.phraseEnd) ohAmt += fill * 0.3f;

        bool wantLow = false;
        float lowVel = 0.55f;

        if (rollLeft_ > 0 && !hasKick) {
            wantLow = true;
            lowVel = 0.42f;
            --rollLeft_;
        }

        if (!wantLow && lastKickStep_ >= 0) {
            const int64_t d = stepIndex - lastKickStep_;
            if ((d == 2 || d == 3) && !hasKick
                && unit(static_cast<uint32_t>(stepIndex), 401) < lowAmt * (d == 2 ? 0.6f : 0.45f)) {
                wantLow = true;
                lowVel = 0.55f;
            }
        }

        if (!wantLow && !hasKick && (s == 1 || s == 5 || s == 11 || s == 14)) {
            if (unit(static_cast<uint32_t>(stepIndex), 411) < lowAmt * 0.4f + sync * 0.15f) {
                wantLow = true;
                lowVel = 0.5f;
            }
        }

        if (!wantLow && ph.phraseEnd && (s >= 12) && !hasKick
            && unit(static_cast<uint32_t>(stepIndex), 421) < fill * 0.45f + lowAmt * 0.2f) {
            wantLow = true;
            lowVel = 0.85f;
        }

        if (!wantLow && dens > 0.75f && !hasKick && (s == 4 || s == 9)
            && unit(static_cast<uint32_t>(stepIndex), 431) < (dens - 0.75f) * 0.6f) {
            wantLow = true;
            rollLeft_ = 1 + static_cast<int>(unit(static_cast<uint32_t>(stepIndex), 432) * 2.0f);
            lowVel = 0.48f;
        }

        if (wantLow) {
            const bool accent = ph.phraseEnd && s >= 14;
            // DNB: rolls / broken replies → tom; phrase-end / sub punctuation → boom
            LowArticulation art = LowArticulation::boom;
            if (rollLeft_ > 0 || (s == 1 || s == 5 || s == 11) || dens > 0.7f)
                art = LowArticulation::tom;
            if (accent || (s >= 12 && ph.phraseEnd))
                art = LowArticulation::boom;
            emitLow(out, offset, lowVel, accent, art);
        }

        if (!hasSnare) {
            float chance = 0.0f;
            if (s == 2 || s == 5 || s == 11)
                chance = midAmt * 0.4f;
            if (s == 3 || s == 7 || s == 13)
                chance = midAmt * 0.35f + sync * 0.2f;
            if (ph.phraseEnd && (s & 1))
                chance += fill * 0.25f;
            if (unit(static_cast<uint32_t>(stepIndex), 441) < chance)
                emitEvent(out, DrumVoice::midPerc, offset,
                          0.35f + 0.35f * unit(static_cast<uint32_t>(stepIndex), 442), false);
        }

        bool wantOH = false;
        float ohVel = 0.65f;
        if ((s == 2 || s == 6 || s == 10 || s == 14)
            && unit(static_cast<uint32_t>(stepIndex), 451) < ohAmt * 0.45f)
            wantOH = true;
        if (!wantOH && ph.phraseEnd && s == 14
            && unit(static_cast<uint32_t>(stepIndex), 461) < fill * 0.4f) {
            wantOH = true;
            ohVel = 0.85f;
        }
        if (wantOH && unit(static_cast<uint32_t>(stepIndex), 471) < var * 0.08f)
            wantOH = false;

        if (wantOH) {
            resolveHatExclusivity(out, from, offset, true);
            emitEvent(out, DrumVoice::openHat, offset, ohVel, s == 14);
        }
    }

    DnbSequencerMode core_;
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
