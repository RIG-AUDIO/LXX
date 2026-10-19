#pragma once

#include "sequencer/dnb_extended_sequencer.h"
#include "sequencer/dnb_sequencer.h"
#include "sequencer/grids_sequencer.h"
#include "sequencer/legacy_sequencer_mode.h"
#include "sequencer/sequencer_mode.h"
#include "sequencer/techno_extended_sequencer.h"
#include "sequencer/techno_sequencer.h"

#include <cmath>

namespace lxx {
namespace dsp {

class SequencerRouter {
public:
    void prepare(double sampleRate)
    {
        sampleRate_ = sampleRate;
        legacy_.prepare(sampleRate);
        grids_.prepare(sampleRate);
        techno_.prepare(sampleRate);
        technoExt_.prepare(sampleRate);
        dnb_.prepare(sampleRate);
        dnbExt_.prepare(sampleRate);
        active_ = SequencerModeId::legacy;
        pending_ = active_;
        switchAtBar_ = false;
    }

    void reset()
    {
        legacy_.reset();
        grids_.reset();
        techno_.reset();
        technoExt_.reset();
        dnb_.reset();
        dnbExt_.reset();
        active_ = pending_;
        switchAtBar_ = false;
        lastBarIndex_ = -1;
    }

    void setParams(const SequencerModeParams& params)
    {
        params_ = params;
        legacy_.setParams(params);
        grids_.setParams(params);
        techno_.setParams(params);
        technoExt_.setParams(params);
        dnb_.setParams(params);
        dnbExt_.setParams(params);

        const auto want = params.mode;
        if (!running_) {
            active_ = want;
            pending_ = want;
            switchAtBar_ = false;
        } else if (want != active_) {
            pending_ = want;
            switchAtBar_ = true;
        }
    }

    void setRunning(bool running)
    {
        running_ = running;
        if (!running) {
            active_ = pending_;
            switchAtBar_ = false;
        }
        activeMode().setRunning(running);
        if (switchAtBar_ && running)
            mode(pending_).setRunning(false);
    }

    uint32_t process(uint32_t numSamples, const HostClockInput& host, DrumEventBuffer& out)
    {
        maybeCommitModeSwitch(host);
        return activeMode().process(numSamples, host, out);
    }

    uint8_t grooveStep() const { return modeConst(active_).grooveStep(); }
    SequencerModeId activeModeId() const noexcept { return active_; }
    LegacySequencerMode& legacy() noexcept { return legacy_; }

private:
    ISequencerMode& activeMode() noexcept { return mode(active_); }

    ISequencerMode& mode(SequencerModeId id) noexcept
    {
        switch (id) {
            case SequencerModeId::grids: return grids_;
            case SequencerModeId::techno: return techno_;
            case SequencerModeId::technoExtended: return technoExt_;
            case SequencerModeId::dnb: return dnb_;
            case SequencerModeId::dnbExtended: return dnbExt_;
            case SequencerModeId::legacy:
            default: return legacy_;
        }
    }

    const ISequencerMode& modeConst(SequencerModeId id) const noexcept
    {
        switch (id) {
            case SequencerModeId::grids: return grids_;
            case SequencerModeId::techno: return techno_;
            case SequencerModeId::technoExtended: return technoExt_;
            case SequencerModeId::dnb: return dnb_;
            case SequencerModeId::dnbExtended: return dnbExt_;
            case SequencerModeId::legacy:
            default: return legacy_;
        }
    }

    void maybeCommitModeSwitch(const HostClockInput& host)
    {
        if (!switchAtBar_ || pending_ == active_)
            return;

        double ppq = 0.0;
        if (host.enabled && host.hasPpq)
            ppq = host.ppq;
        const int64_t bar = static_cast<int64_t>(std::floor(ppq / 4.0));
        if (lastBarIndex_ < 0) {
            lastBarIndex_ = bar;
            return;
        }
        if (bar != lastBarIndex_) {
            mode(active_).setRunning(false);
            active_ = pending_;
            mode(active_).reset();
            mode(active_).setParams(params_);
            mode(active_).setRunning(running_);
            switchAtBar_ = false;
        }
        lastBarIndex_ = bar;
    }

    SequencerModeParams params_ {};
    LegacySequencerMode legacy_;
    GridsSequencerMode grids_;
    TechnoSequencerMode techno_;
    TechnoExtendedSequencerMode technoExt_;
    DnbSequencerMode dnb_;
    DnbExtendedSequencerMode dnbExt_;
    SequencerModeId active_ = SequencerModeId::legacy;
    SequencerModeId pending_ = SequencerModeId::legacy;
    bool running_ = false;
    bool switchAtBar_ = false;
    int64_t lastBarIndex_ = -1;
    double sampleRate_ = 48000.0;
};

}  // namespace dsp
}  // namespace lxx
