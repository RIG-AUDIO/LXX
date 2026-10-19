#pragma once

#include "sequencer/sequencer_mode.h"

#include <cmath>
#include <cstdint>

namespace lxx {
namespace dsp {

// Original techno generative grammar: 4/4 gravity, phrase-aware fills, offbeat hats.
class TechnoSequencerMode final : public ISequencerMode {
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
        constexpr double kPpqPerStep = 0.25; // 16th grid
        const double ppqPerSample = bpm / (60.0 * sr);

        double blockStartPpq = 0.0;
        if (host.enabled && host.hasPpq) {
            blockStartPpq = host.ppq;
        } else {
            blockStartPpq = internalPpq_;
            internalPpq_ += ppqPerSample * static_cast<double>(numSamples);
        }

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

            // Swing delays odd 16ths
            const bool odd16 = (step & 1) != 0;
            if (odd16 && params_.technoSwing > 0.0f) {
                stepPpq += kPpqPerStep * static_cast<double>(params_.technoSwing) * 0.45;
            }

            int offset = static_cast<int>(std::llround((stepPpq - blockStartPpq) / ppqPerSample));
            if (offset < 0)
                offset = 0;
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
    SequencerModeId id() const override { return SequencerModeId::techno; }

private:
    uint32_t hash(uint32_t x) const noexcept
    {
        x ^= params_.technoSeed * 0x9E3779B9u;
        x ^= x >> 16;
        x *= 0x7FEB352Du;
        x ^= x >> 15;
        x *= 0x846CA68Bu;
        x ^= x >> 16;
        return x;
    }

    float unit(uint32_t step, uint32_t salt) const noexcept
    {
        return static_cast<float>((hash(static_cast<uint32_t>(step) ^ salt) >> 8) & 0x00ffffffu)
            * (1.0f / 16777216.0f);
    }

    void maybeEmit(DrumEventBuffer& out, DrumVoice voice, uint32_t offset, float vel, bool accent)
    {
        DrumEvent e;
        e.voice = voice;
        e.sampleOffset = offset;
        e.velocity = vel;
        e.accent = accent;
        out.push(e);
    }

    void evaluateStep(int64_t stepIndex, uint32_t offset, DrumEventBuffer& out)
    {
        const int stepInBar = static_cast<int>(stepIndex & 15);       // 16 steps / bar
        const int barInPhrase = static_cast<int>((stepIndex / 16) & 3); // 4-bar phrase
        const int stepInPhrase = static_cast<int>(stepIndex & 63);      // 64 16ths = 4 bars
        const bool phraseEndZone = stepInPhrase >= 56;                  // last half-bar of phrase
        const bool barEndZone = stepInBar >= 14;

        const float dens = params_.technoDensity;
        const float var = params_.technoVariation;
        const float sync = params_.technoSyncopation;
        const float fill = params_.technoFill;

        // --- KICK: 4-on-the-floor attractor ---
        bool kick = (stepInBar == 0 || stepInBar == 4 || stepInBar == 8 || stepInBar == 12);
        float kickVel = 1.0f;
        if (kick) {
            // Velocity structure: bar1 downbeat strongest; subtle phrase shading
            if (stepInBar == 0)
                kickVel = (barInPhrase == 0) ? 1.0f : 0.95f;
            else
                kickVel = 0.88f + 0.08f * unit(static_cast<uint32_t>(stepIndex), 7);
            // Occasional intentional omission only at high variation, never downbeat of bar 1
            if (!(barInPhrase == 0 && stepInBar == 0) && unit(static_cast<uint32_t>(stepIndex), 11) < var * 0.1f)
                kick = false;
        } else {
            const bool eighthOff = (stepInBar == 2 || stepInBar == 6 || stepInBar == 10 || stepInBar == 14);
            const bool sixteenthPickup = (stepInBar == 15 || stepInBar == 7 || stepInBar == 3);
            float chance = 0.0f;
            if (eighthOff)
                chance = dens * 0.2f;
            if (sixteenthPickup)
                chance = dens * 0.16f + sync * 0.18f;
            if (phraseEndZone)
                chance += fill * 0.28f;
            if (unit(static_cast<uint32_t>(stepIndex), 21) < chance) {
                kick = true;
                kickVel = 0.65f + 0.15f * unit(static_cast<uint32_t>(stepIndex), 22);
            }
        }
        if (kick)
            maybeEmit(out, DrumVoice::kick, offset, kickVel, stepInBar == 0);

        // --- SNARE/CLAP: backbeat gravity on 2 and 4 + restrained ghosts ---
        bool snare = (stepInBar == 4 || stepInBar == 12);
        float snareVel = 0.92f;
        if (snare) {
            snareVel = 0.88f + 0.1f * unit(static_cast<uint32_t>(stepIndex), 29);
            if (unit(static_cast<uint32_t>(stepIndex), 31) < var * 0.06f)
                snare = false;
        } else {
            float chance = 0.0f;
            if (stepInBar == 3 || stepInBar == 11)
                chance = dens * 0.1f + sync * 0.22f; // pre-backbeat ghost
            if (stepInBar == 5 || stepInBar == 13)
                chance = dens * 0.07f + sync * 0.14f;
            if (stepInBar == 10 || stepInBar == 6)
                chance = dens * 0.08f + sync * 0.12f;
            if (phraseEndZone && barEndZone)
                chance += fill * 0.38f;
            if (unit(static_cast<uint32_t>(stepIndex), 41) < chance) {
                snare = true;
                snareVel = phraseEndZone ? 0.82f : (0.35f + 0.2f * unit(static_cast<uint32_t>(stepIndex), 42));
            }
        }
        if (snare)
            maybeEmit(out, DrumVoice::snare, offset, snareVel, stepInBar == 4 || stepInBar == 12);

        // --- HAT: offbeat 8ths core, accent cycles, 16th drive ---
        bool hat = false;
        float hatVel = 0.55f;
        bool hatAccent = false;
        const bool offbeat8 = (stepInBar == 2 || stepInBar == 6 || stepInBar == 10 || stepInBar == 14);
        const bool onbeat8 = (stepInBar == 0 || stepInBar == 4 || stepInBar == 8 || stepInBar == 12);
        const bool odd16 = (stepInBar & 1) != 0;
        // 2-bar accent cycle on offbeats
        const bool accentCycle = ((stepIndex / 16) & 1) == 0;

        if (offbeat8) {
            hat = true;
            hatVel = accentCycle ? 0.78f : 0.62f;
            hatAccent = accentCycle || stepInBar == 2 || stepInBar == 10;
            if (unit(static_cast<uint32_t>(stepIndex), 51) < var * 0.08f)
                hat = false;
        } else if (onbeat8) {
            if (unit(static_cast<uint32_t>(stepIndex), 61) < dens * 0.32f) {
                hat = true;
                hatVel = 0.38f;
            }
        } else if (odd16) {
            if (unit(static_cast<uint32_t>(stepIndex), 71) < dens * 0.52f) {
                hat = true;
                hatVel = 0.32f + 0.22f * unit(static_cast<uint32_t>(stepIndex), 81);
            }
        }

        if (!hat && phraseEndZone && odd16
            && unit(static_cast<uint32_t>(stepIndex), 91) < fill * 0.45f) {
            hat = true;
            hatVel = 0.52f;
        }

        if (hat)
            maybeEmit(out, DrumVoice::hat, offset, hatVel, hatAccent);
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
