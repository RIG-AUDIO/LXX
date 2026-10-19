#pragma once

#include "sequencer/sequencer_mode.h"
#include "sequencer/transport.h"

namespace lxx {
namespace dsp {

class LegacySequencerMode final : public ISequencerMode {
public:
    void prepare(double sampleRate) override
    {
        sampleRate_ = sampleRate > 1.0 ? sampleRate : 48000.0;
        transport_.prepare(sampleRate_);
        reset();
    }

    void reset() override
    {
        transport_.reset();
        running_ = false;
    }

    void setParams(const SequencerModeParams& params) override
    {
        params_ = params;
        SequencerSettings s;
        s.tempo = params.tempo;
        s.hostSync = params.hostSync;
        s.clockDivision = params.clockDivision;
        s.swing = params.swing;
        s.drumsX = params.drumsX;
        s.drumsY = params.drumsY;
        s.drumsDensity = params.density;
        // Keep tones/balance unused for sequencing
        transport_.setSettings(s);
    }

    void setRunning(bool running) override
    {
        if (running && !running_)
            transport_.start();
        else if (!running && running_)
            transport_.stop();
        running_ = running;
    }

    uint32_t process(uint32_t numSamples, const HostClockInput& host, DrumEventBuffer& out) override
    {
        HostClockInput h = host;
        h.divisionIndex = params_.clockDivision;
        if (!(h.sampleRate > 1.0))
            h.sampleRate = sampleRate_;

        const auto events = transport_.processBlock(numSamples, h);
        uint32_t n = 0;
        for (const auto& d : events.drums) {
            DrumEvent e;
            e.voice = voiceFromLegacyPart(d.part);
            e.sampleOffset = 0; // legacy transport is step-synchronous per block
            e.velocity = static_cast<float>(d.level) / 255.0f;
            e.accent = d.level > 200;
            out.push(e);
            ++n;
        }
        return n;
    }

    uint8_t grooveStep() const override { return transport_.grooveStep(); }
    SequencerModeId id() const override { return SequencerModeId::legacy; }
    SequencerTransport& transport() noexcept { return transport_; }

private:
    SequencerModeParams params_ {};
    SequencerTransport transport_;
    double sampleRate_ = 48000.0;
    bool running_ = false;
};

}  // namespace dsp
}  // namespace lxx
