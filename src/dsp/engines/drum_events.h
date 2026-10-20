#pragma once

#include <array>
#include <cstdint>

#include "sequencer/sequencer_types.h"

namespace lxx {
namespace dsp {

enum class DrumVoice : uint8_t {
    lowTom = 0,   // B0 / 35 — LOW lane (Boom / Low Tom articulations)
    kick = 1,     // C1 / 36
    snare = 2,    // D1 / 38
    midPerc = 3,  // D#1 / 39
    hat = 4,      // F#1 / 42 closed
    openHat = 5,  // A#1 / 46
    count = 6
};

/** Single native LXX drum engine (Analog removed; id retained for state migration). */
enum class DrumEngineId : uint8_t {
    native = 0,
    legacy = 0,   // alias
    analog = 1    // obsolete — maps to native at load/runtime
};

enum class SequencerModeId : uint8_t {
    legacy = 0,
    grids = 1,
    techno = 2,
    technoExtended = 3,
    dnb = 4,
    dnbExtended = 5
};

/** Boom ↔ Tom articulation for the LOW MIDI lane (same note 35). */
enum class LowArticulation : uint8_t {
    boom = 0,
    tom = 1
};

// Note numbering with C3=60: B0=35, C1=36, D1=38, D#1=39, F#1=42, A#1=46
static constexpr int kMidiNoteLowTom = 35;
static constexpr int kMidiNoteKick = 36;
static constexpr int kMidiNoteSnare = 38;
static constexpr int kMidiNoteMidPerc = 39;
static constexpr int kMidiNoteHat = 42;
static constexpr int kMidiNoteOpenHat = 46;

struct DrumEvent {
    DrumVoice voice = DrumVoice::kick;
    uint32_t sampleOffset = 0;
    float velocity = 1.0f;
    bool accent = false;
    LowArticulation lowArticulation = LowArticulation::boom;
};

static constexpr uint32_t kMaxDrumEventsPerBlock = 96;

struct DrumEventBuffer {
    std::array<DrumEvent, kMaxDrumEventsPerBlock> events {};
    uint32_t count = 0;

    void clear() noexcept { count = 0; }

    bool push(const DrumEvent& e) noexcept
    {
        if (count >= kMaxDrumEventsPerBlock)
            return false;
        events[count++] = e;
        return true;
    }
};

inline int midiNoteForVoice(DrumVoice v) noexcept
{
    switch (v) {
        case DrumVoice::lowTom: return kMidiNoteLowTom;
        case DrumVoice::kick: return kMidiNoteKick;
        case DrumVoice::snare: return kMidiNoteSnare;
        case DrumVoice::midPerc: return kMidiNoteMidPerc;
        case DrumVoice::hat: return kMidiNoteHat;
        case DrumVoice::openHat: return kMidiNoteOpenHat;
        case DrumVoice::count: break;  // sentinel, never a real voice
    }
    return kMidiNoteKick;
}

inline uint8_t midiVelocityFromEvent(const DrumEvent& e) noexcept
{
    const float v = e.velocity <= 0.0f ? 0.0f : (e.velocity >= 1.0f ? 1.0f : e.velocity);
    int out = static_cast<int>(v * 127.0f + 0.5f);
    if (e.accent && out < 100)
        out = out + ((127 - out) / 3);
    if (out < 1) out = 1;
    if (out > 127) out = 127;
    return static_cast<uint8_t>(out);
}

inline int legacyPartForVoice(DrumVoice v) noexcept
{
    switch (v) {
        case DrumVoice::kick: return 0;
        case DrumVoice::snare: return 1;
        case DrumVoice::hat: return 2;
        case DrumVoice::lowTom:
        case DrumVoice::midPerc:
        case DrumVoice::openHat:
        case DrumVoice::count:
            return -1;  // extended-only parts have no legacy part
    }
    return -1;  // unreachable when the switch is exhaustive; every path returns
}

inline DrumVoice voiceFromLegacyPart(uint8_t part) noexcept
{
    switch (part % 3) {
        case 0: return DrumVoice::kick;
        case 1: return DrumVoice::snare;
        default: return DrumVoice::hat;
    }
}

}  // namespace dsp
}  // namespace lxx
