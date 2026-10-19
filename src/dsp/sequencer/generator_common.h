#pragma once

#include "engines/drum_events.h"
#include "sequencer/sequencer_mode.h"

#include <cstdint>

namespace lxx {
namespace dsp {

/** Shared deterministic hash for generative sequencers. */
inline uint32_t genHash(uint32_t x, uint32_t seed) noexcept
{
    x ^= seed * 0x9E3779B9u;
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

inline float genUnit(uint32_t step, uint32_t salt, uint32_t seed) noexcept
{
    return static_cast<float>((genHash(step ^ salt, seed) >> 8) & 0x00ffffffu)
        * (1.0f / 16777216.0f);
}

struct PhraseContext {
    int stepInBar = 0;      // 0..15
    int barInPhrase = 0;    // 0..7 (8-bar) or 0..3 (4-bar)
    int stepInPhrase = 0;
    bool phraseEarly = false;
    bool phraseMid = false;
    bool phraseEnd = false;
    bool barEnd = false;
};

inline PhraseContext makePhrase4(int64_t stepIndex) noexcept
{
    PhraseContext c;
    c.stepInBar = static_cast<int>(stepIndex & 15);
    c.barInPhrase = static_cast<int>((stepIndex / 16) & 3);
    c.stepInPhrase = static_cast<int>(stepIndex & 63);
    c.phraseEarly = c.barInPhrase == 0;
    c.phraseMid = c.barInPhrase == 1 || c.barInPhrase == 2;
    c.phraseEnd = c.stepInPhrase >= 56;
    c.barEnd = c.stepInBar >= 14;
    return c;
}

inline PhraseContext makePhrase8(int64_t stepIndex) noexcept
{
    PhraseContext c;
    c.stepInBar = static_cast<int>(stepIndex & 15);
    c.barInPhrase = static_cast<int>((stepIndex / 16) & 7);
    c.stepInPhrase = static_cast<int>(stepIndex & 127);
    c.phraseEarly = c.barInPhrase <= 1;
    c.phraseMid = c.barInPhrase >= 2 && c.barInPhrase <= 4;
    c.phraseEnd = c.stepInPhrase >= 112;
    c.barEnd = c.stepInBar >= 14;
    return c;
}

inline void emitEvent(DrumEventBuffer& out, DrumVoice v, uint32_t off, float vel, bool accent,
                      LowArticulation lowArt = LowArticulation::boom)
{
    DrumEvent e;
    e.voice = v;
    e.sampleOffset = off;
    e.velocity = vel;
    e.accent = accent;
    e.lowArticulation = lowArt;
    out.push(e);
}

/** Emit LOW lane with Boom/Tom articulation (same MIDI note 35). */
inline void emitLow(DrumEventBuffer& out, uint32_t off, float vel, bool accent, LowArticulation art)
{
    emitEvent(out, DrumVoice::lowTom, off, vel, accent, art);
}

/** Remove all events of voice v at exact sampleOffset within [from, count). */
inline void removeVoiceAtOffset(DrumEventBuffer& out, uint32_t from, DrumVoice v, uint32_t offset)
{
    uint32_t w = from;
    for (uint32_t i = from; i < out.count; ++i) {
        if (out.events[i].voice == v && out.events[i].sampleOffset == offset)
            continue;
        if (w != i)
            out.events[w] = out.events[i];
        ++w;
    }
    out.count = w;
}

inline bool hasVoiceAtOffset(const DrumEventBuffer& out, uint32_t from, DrumVoice v, uint32_t offset)
{
    for (uint32_t i = from; i < out.count; ++i)
        if (out.events[i].voice == v && out.events[i].sampleOffset == offset)
            return true;
    return false;
}

/**
 * Hard CH/OH mutual exclusion at a sample offset.
 * Priority: if wantOpenHat, keep OH and suppress CH; else keep CH and suppress OH.
 */
inline void resolveHatExclusivity(DrumEventBuffer& out, uint32_t from, uint32_t offset, bool wantOpenHat)
{
    if (wantOpenHat) {
        removeVoiceAtOffset(out, from, DrumVoice::hat, offset);
        // OH emit is caller's responsibility if not already present
    } else {
        removeVoiceAtOffset(out, from, DrumVoice::openHat, offset);
    }
}

/** Sweep buffer and ensure no offset has both CH and OH (OH wins). */
inline void enforceHatExclusivityPass(DrumEventBuffer& out, uint32_t from = 0)
{
    for (uint32_t i = from; i < out.count; ++i) {
        if (out.events[i].voice != DrumVoice::openHat)
            continue;
        removeVoiceAtOffset(out, from, DrumVoice::hat, out.events[i].sampleOffset);
    }
}

}  // namespace dsp
}  // namespace lxx
