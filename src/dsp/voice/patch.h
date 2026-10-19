#pragma once

#include "../core/dsp_types.h"
#include "../filters/svf_anushri.h"

namespace lxx {
namespace dsp {

/**
 * Runtime patch — float params mapped from firmware Patch (upstream anu/voice.h).
 * Plugin APVTS values are converted here in Voice::setPatch().
 */
struct VoicePatch {
    // VCO
    Sample oscRange = 0.0f;          // -2..+2 oct (vco_dco_range)
    Sample oscDcoFine = 0.0f;        // ±50 cents
    Sample oscDetune = 0.0f;         // ±100 cents (unison spread also uses this)
    Sample oscFine = 0.0f;           // ±50 cents
    Sample pitchEnvAmt = 0.0f;       // 0..1
    Sample pitchLfoAmt = 0.0f;       // 0..1

    // PWM
    Sample pwEnvAmt = 0.0f;
    Sample pwLfoAmt = 0.0f;

    // Mixer
    Sample waveLevel = 0.8f;
    Sample subLevel = 0.5f;

    // VCF
    Sample filterCutoffHz = 8000.0f; // panel cutoff (no firmware EEPROM field)
    Sample filterResonance = 0.2f;   // analog RES pot (0..1)
    SVFOutput filterMode = SVFOutput::LOWPASS;
    Sample filterKbdTrack = 1.0f;    // 0..2 (100% = 1.0)
    Sample filterEnvAmt = 0.0f;
    Sample filterLfoAmt = 0.0f;
    Sample filterModAmt = 0.0f;      // growl / mod wheel proxy

    // Envelopes (seconds / levels)
    Sample envAttack = 0.001f;
    Sample envDecay = 0.2f;
    Sample envSustain = 0.5f;
    Sample envRelease = 0.3f;
    Sample envVcaMorph = 0.5f;       // firmware init_patch env_vca_morph = 128
    bool envLegatoMode = false;

    // LFO
    uint8_t lfoShape = 0;
    Sample lfoRateHz = 2.0f;
    Sample vibratoRateHz = 5.0f;
    Sample vibratoDest = 0.0f;       // 0=pitch, 1=growl (firmware vibrato_destination)

    // Keyboard
    Sample glideTimeSec = 0.0f;      // 0..6 s
    Sample velToFilter = 0.0f;
    Sample velToAmp = 0.0f;

    // Global
    Sample volume = 0.8f;            // 0..1 (firmware 0..255)
    bool authenticMode = true;

    // Extended (plugin)
    int unisonCount = 1;             // 1..4
    Sample unisonDetuneCents = 15.0f;  // spread half-width in cents

    bool operator==(const VoicePatch& other) const = default;
    bool operator!=(const VoicePatch& other) const = default;
};

} // namespace dsp
} // namespace lxx
