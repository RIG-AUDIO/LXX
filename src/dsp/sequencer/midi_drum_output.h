#pragma once

#include "engines/drum_events.h"

#include <array>
#include <cstdint>

namespace lxx {
namespace dsp {

// Schedules sample-accurate MIDI note-ons/offs for drum events across block boundaries.
class MidiDrumOutput {
public:
    void prepare(double sampleRate)
    {
        sampleRate_ = sampleRate > 1.0 ? sampleRate : 48000.0;
        noteLengthSamples_ = static_cast<uint32_t>(0.04 * sampleRate_); // 40 ms
        if (noteLengthSamples_ < 8)
            noteLengthSamples_ = 8;
        reset();
    }

    void reset()
    {
        for (auto& s : slots_) {
            s.active = false;
            s.note = 0;
            s.offCountdown = 0;
        }
        pendingOffs_ = 0;
    }

    void setEnabled(bool enabled) noexcept { enabled_ = enabled; }
    bool enabled() const noexcept { return enabled_; }

    struct MidiMessage {
        bool isNoteOn = false;
        uint8_t note = 0;
        uint8_t velocity = 0;
        uint32_t sampleOffset = 0;
    };

    static constexpr uint32_t kMaxMessages = 96;
    struct MidiMessageBuffer {
        std::array<MidiMessage, kMaxMessages> msgs {};
        uint32_t count = 0;
        void clear() noexcept { count = 0; }
        bool push(const MidiMessage& m) noexcept
        {
            if (count >= kMaxMessages)
                return false;
            msgs[count++] = m;
            return true;
        }
    };

    void process(const DrumEventBuffer& events, uint32_t numSamples, MidiMessageBuffer& midiOut)
    {
        midiOut.clear();
        if (!enabled_) {
            return;
        }

        // Emit pending note-offs that fall in this block
        for (auto& s : slots_) {
            if (!s.active)
                continue;
            if (s.offCountdown < numSamples) {
                MidiMessage m;
                m.isNoteOn = false;
                m.note = s.note;
                m.velocity = 0;
                m.sampleOffset = s.offCountdown;
                midiOut.push(m);
                s.active = false;
            } else {
                s.offCountdown -= numSamples;
            }
        }

        for (uint32_t i = 0; i < events.count; ++i) {
            const auto& e = events.events[i];
            const uint8_t note = static_cast<uint8_t>(midiNoteForVoice(e.voice));
            const uint8_t vel = midiVelocityFromEvent(e);
            uint32_t offAt = e.sampleOffset + noteLengthSamples_;

            // If same note still active, note-off immediately before new on
            for (auto& s : slots_) {
                if (s.active && s.note == note) {
                    MidiMessage off;
                    off.isNoteOn = false;
                    off.note = note;
                    off.velocity = 0;
                    off.sampleOffset = e.sampleOffset;
                    midiOut.push(off);
                    s.active = false;
                }
            }

            MidiMessage on;
            on.isNoteOn = true;
            on.note = note;
            on.velocity = vel;
            on.sampleOffset = e.sampleOffset;
            midiOut.push(on);

            Slot* slot = acquire();
            if (slot != nullptr) {
                slot->active = true;
                slot->note = note;
                if (offAt < numSamples) {
                    MidiMessage off;
                    off.isNoteOn = false;
                    off.note = note;
                    off.velocity = 0;
                    off.sampleOffset = offAt;
                    midiOut.push(off);
                    slot->active = false;
                } else {
                    slot->offCountdown = offAt - numSamples;
                }
            }
        }
    }

    void flushAll(MidiMessageBuffer& midiOut, uint32_t offset = 0)
    {
        for (auto& s : slots_) {
            if (!s.active)
                continue;
            MidiMessage m;
            m.isNoteOn = false;
            m.note = s.note;
            m.velocity = 0;
            m.sampleOffset = offset;
            midiOut.push(m);
            s.active = false;
        }
        pendingOffs_ = 0;
    }

private:
    struct Slot {
        bool active = false;
        uint8_t note = 0;
        uint32_t offCountdown = 0;
    };

    Slot* acquire()
    {
        for (auto& s : slots_) {
            if (!s.active)
                return &s;
        }
        return nullptr;
    }

    bool enabled_ = false;
    double sampleRate_ = 48000.0;
    uint32_t noteLengthSamples_ = 1920;
    std::array<Slot, 8> slots_ {};
    uint32_t pendingOffs_ = 0;
};

}  // namespace dsp
}  // namespace lxx
