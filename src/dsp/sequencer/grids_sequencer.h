#pragma once

#include "sequencer/grids/grids_pattern_generator.h"
#include "sequencer/sequencer_mode.h"

#include <cmath>

namespace lxx {
namespace dsp {

class GridsSequencerMode final : public ISequencerMode {
public:
    void prepare(double sampleRate) override
    {
        sampleRate_ = sampleRate > 1.0 ? sampleRate : 48000.0;
        reset();
    }

    void reset() override
    {
        gen_.reset();
        gen_.setSeed(0x651D50u);
        lastStepIndex_ = -1;
        wasPlaying_ = false;
        grooveStep_ = 0;
        internalPpq_ = 0.0;
    }

    void setParams(const SequencerModeParams& params) override
    {
        params_ = params;
        grids::GridsSettings gs;
        gs.x = params.drumsX;
        gs.y = params.drumsY;
        gs.randomness = params.gridsChaos;
        gs.density = params.density;
        gen_.setSettings(gs);
    }

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
        constexpr double kPpqPerStep = 0.25; // 16th notes
        const double ppqPerSample = bpm / (60.0 * sr);

        double blockStartPpq = 0.0;
        if (host.enabled && host.hasPpq) {
            blockStartPpq = host.ppq;
        } else {
            blockStartPpq = internalPpq_;
            internalPpq_ += ppqPerSample * static_cast<double>(numSamples);
        }

        if (!wasPlaying_) {
            // On transport start/seek: snap generator to pattern position without emitting.
            const int64_t stepNow = static_cast<int64_t>(std::floor(blockStartPpq / kPpqPerStep));
            gen_.reset();
            gen_.setSeed(0x651D50u ^ static_cast<uint32_t>(stepNow));
            for (int64_t s = 0; s < (stepNow % grids::kStepsPerPattern); ++s)
                gen_.evaluateStep();
            lastStepIndex_ = stepNow - 1;
            wasPlaying_ = true;
        }

        const double blockEndPpq = blockStartPpq + ppqPerSample * static_cast<double>(numSamples);
        int64_t step = lastStepIndex_ + 1;
        const int64_t endStep = static_cast<int64_t>(std::floor((blockEndPpq - 1.0e-12) / kPpqPerStep));

        uint32_t produced = 0;
        for (; step <= endStep; ++step) {
            const double stepPpq = static_cast<double>(step) * kPpqPerStep;
            int offset = static_cast<int>(std::llround((stepPpq - blockStartPpq) / ppqPerSample));
            if (offset < 0)
                offset = 0;
            if (offset >= static_cast<int>(numSamples))
                continue;

            const auto result = gen_.evaluateStep();
            grooveStep_ = result.step;
            for (uint8_t i = 0; i < 3; ++i) {
                const uint8_t mask = static_cast<uint8_t>(1u << i);
                if ((result.triggers & mask) == 0)
                    continue;
                DrumEvent e;
                e.voice = voiceFromLegacyPart(i);
                e.sampleOffset = static_cast<uint32_t>(offset);
                e.accent = (result.accents & mask) != 0;
                e.velocity = e.accent ? 1.0f : 0.72f;
                out.push(e);
            }
            lastStepIndex_ = step;
            ++produced;
        }
        return produced;
    }

    uint8_t grooveStep() const override { return grooveStep_; }
    SequencerModeId id() const override { return SequencerModeId::grids; }

private:
    SequencerModeParams params_ {};
    grids::PatternGenerator gen_;
    double sampleRate_ = 48000.0;
    double internalPpq_ = 0.0;
    bool running_ = false;
    bool wasPlaying_ = false;
    int64_t lastStepIndex_ = -1;
    uint8_t grooveStep_ = 0;
};

}  // namespace dsp
}  // namespace lxx
