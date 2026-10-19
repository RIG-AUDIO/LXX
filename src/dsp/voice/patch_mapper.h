#pragma once

#include "voice/patch.h"
#include "filters/svf_anushri.h"
#include <algorithm>

namespace lxx {
namespace dsp {

/** Build VoicePatch from normalized plugin parameter values (0..1 or physical units). */
inline VoicePatch makeVoicePatch(
    bool authenticMode,
    float vcoDcoRange,
    float vcoDcoFine,
    float vcoDetune,
    float vcoFine,
    float vcoEnvAmount,
    float vcoLfoAmount,
    float pwEnvAmount,
    float pwLfoAmount,
    float waveLevel,
    float subLevel,
    float filterCutoffHz,
    float filterResonance,
    int filterModeIndex,
    float filterKbdTrackPct,
    float filterEnvAmount,
    float filterLfoAmount,
    float filterModAmount,
    float envAttack,
    float envDecay,
    float envSustain,
    float envRelease,
    float envVcaMorph,
    int lfoShape,
    float lfoRateHz,
    float volume,
    float glideSec,
    float velToFilter = 0.0f,
    float velToAmp = 0.0f,
    bool legatoMode = false,
    int unisonCount = 1,
    float unisonDetuneCents = 15.0f) {
    VoicePatch p;
    p.authenticMode = authenticMode;
    p.oscRange = vcoDcoRange;
    p.oscDcoFine = vcoDcoFine;
    p.oscDetune = vcoDetune;
    p.oscFine = vcoFine;
    p.pitchEnvAmt = vcoEnvAmount;
    p.pitchLfoAmt = vcoLfoAmount;
    p.pwEnvAmt = pwEnvAmount;
    p.pwLfoAmt = pwLfoAmount;
    p.waveLevel = waveLevel;
    p.subLevel = subLevel;
    p.filterCutoffHz = filterCutoffHz;
    p.filterResonance = filterResonance;
    p.filterMode = static_cast<SVFOutput>(std::clamp(filterModeIndex, 0, 2));
    p.filterKbdTrack = filterKbdTrackPct / 100.0f;
    p.filterEnvAmt = filterEnvAmount;
    p.filterLfoAmt = filterLfoAmount;
    p.filterModAmt = filterModAmount;
    p.envAttack = envAttack;
    p.envDecay = envDecay;
    p.envSustain = envSustain;
    p.envRelease = envRelease;
    p.envVcaMorph = envVcaMorph;
    p.envLegatoMode = legatoMode;
    p.lfoShape = static_cast<uint8_t>(std::clamp(lfoShape, 0, 7));
    p.lfoRateHz = lfoRateHz;
    p.vibratoRateHz = 5.0f;
    p.glideTimeSec = glideSec;
    p.velToFilter = velToFilter;
    p.velToAmp = velToAmp;
    p.volume = volume;
    p.unisonCount = std::clamp(unisonCount, 1, 4);
    p.unisonDetuneCents = unisonDetuneCents;
    return p;
}

} // namespace dsp
} // namespace lxx
