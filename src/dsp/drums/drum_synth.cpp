#include "drum_synth.h"

#include "../core/dsp_types.h"
#include "drum_resources.h"

#include <algorithm>
#include <cstring>
#include <cmath>

namespace lxx {
namespace dsp {

namespace {

namespace Res = drum_resources;

static const uint8_t kPresetBd1[] = {60, 18, 104, 120, 0};
static const uint8_t kPresetBd2[] = {56, 60, 120, 150, 0};
static const uint8_t kPresetBd3[] = {60, 42, 130, 180, 14};
static const uint8_t kPresetBd4[] = {72, 20, 66, 224, 0};
static const uint8_t kPresetBd5[] = {42, 52, 106, 160, 60};

static const uint8_t kPresetSd1[] = {108, 18, 16, 72, 64};
static const uint8_t kPresetSd2[] = {108, 36, 32, 96, 140};
static const uint8_t kPresetSd3[] = {108, 36, 50, 90, 180};
static const uint8_t kPresetSd4[] = {116, 36, 32, 80, 150};
static const uint8_t kPresetSd5[] = {124, 40, 190, 90, 40};

static const uint8_t kPresetHh1[] = {124, 0, 0, 80, 0};
static const uint8_t kPresetHh2[] = {136, 0, 0, 80, 0};
static const uint8_t kPresetHh3[] = {136, 0, 0, 110, 0};
static const uint8_t kPresetHh4[] = {148, 0, 0, 90, 0};
static const uint8_t kPresetHh5[] = {154, 0, 0, 45, 0};

static const uint8_t* const kDrumPresets[] = {
    kPresetBd1, kPresetBd2, kPresetBd3, kPresetBd4, kPresetBd5,
    kPresetSd1, kPresetSd2, kPresetSd3, kPresetSd4, kPresetSd5,
    kPresetHh1, kPresetHh2, kPresetHh3, kPresetHh4, kPresetHh5,
};

static const uint8_t kDrumsCcMap[] = {
    0, 1, 2, 3, 4, 5,
    6, 7, 8, 9, 10, 11,
    12, 15, 17,
};

// Firmware increments are uint16 per native sample. Convert to 16.16 per host sample
// so pitch/envelope rates stay musically invariant when sample rate changes.
inline uint32_t scaleFirmwareIncrement(uint16_t increment, float scale) {
    const double scaled = static_cast<double>(increment) * static_cast<double>(scale) * 65536.0;
    if (!(scaled > 0.0))
        return 0;
    if (scaled >= 4294967295.0)
        return 0xffffffffu;
    return static_cast<uint32_t>(scaled + 0.5);
}

}  // namespace

DrumSynth::DrumSynth() {
    init(kSampleRate);
}

void DrumSynth::init(float sampleRate) {
    sampleRate_ = sampleRate;
    reset();
    updateRateScaling();

    for (int i = 0; i < kNumDrumInstruments; ++i) {
        morphPatch(static_cast<uint8_t>(i), 128);
        patch_[i].level = 255;
    }
}

void DrumSynth::reset() {
    for (auto& s : state_)
        s = DrumState{};
    sample_ = 128;
    sampleCounter_ = 0;
    sampleRateHold_ = 0;
    fadeCounter_ = 0;
    noiseState_ = 0;
    rngState_ = 0x21;
    playing_ = false;
}

void DrumSynth::seedRandom(uint16_t seed) {
    rngState_ = seed;
}

void DrumSynth::updateBitcrushPeriod() {
    // Firmware counts native samples between DAC updates (bandwidth crush).
    // Preserve that period in seconds at any host rate.
    const float nativePeriod = static_cast<float>(sampleRateHold_) + 1.0f;
    const float hostPeriod = nativePeriod * (sampleRate_ / kFirmwareAudioRate);
    bitcrushPeriod_ = static_cast<uint32_t>(std::max(1.0f, hostPeriod + 0.5f));
}

void DrumSynth::updateRateScaling() {
    // LUT increments are firmware-rate phase steps. Host increment must shrink
    // as sample rate rises so musical pitch stays constant (Martello native-rate
    // engines + SRC: same idea, rate-aware equations here).
    const float sr = (sampleRate_ > 1.0f && std::isfinite(sampleRate_)) ? sampleRate_ : kFirmwareAudioRate;
    audioRateScale_ = kFirmwareAudioRate / sr;
    const float controlRate = sr / static_cast<float>(kAudioBlockSize);
    controlRateScale_ = kFirmwareDrumControlRate / controlRate;
    updateBitcrushPeriod();
}

uint8_t DrumSynth::u8u8MulShift8(uint8_t a, uint8_t b) {
    return static_cast<uint8_t>((static_cast<uint16_t>(a) * b) >> 8);
}

int8_t DrumSynth::s8u8MulShift8(int8_t a, uint8_t b) {
    return static_cast<int8_t>((static_cast<int16_t>(a) * b) >> 8);
}

uint16_t DrumSynth::u8u8Mul(uint8_t a, uint8_t b) {
    return static_cast<uint16_t>(a) * b;
}

uint8_t DrumSynth::u8Mix(uint8_t a, uint8_t b, uint8_t balance) {
    return static_cast<uint8_t>((a * (255 - balance) + b * balance) >> 8);
}

uint8_t DrumSynth::interpolateSample(const uint8_t* table, uint16_t phase) {
    const uint8_t index = static_cast<uint8_t>(phase >> 8);
    return u8Mix(table[index], table[index + 1], static_cast<uint8_t>(phase & 0xff));
}

uint16_t DrumSynth::interpolateIncreasing(const uint16_t* table, uint16_t phase) {
    const uint16_t index = phase >> 8;
    const uint16_t a = table[index];
    const uint16_t b = table[index + 1];
    return static_cast<uint16_t>(a + u8u8MulShift8(static_cast<uint8_t>(b - a),
                                                   static_cast<uint8_t>(phase & 0xff)));
}

void DrumSynth::trigger(uint8_t instrument, uint8_t level) {
    if (instrument >= kNumDrumInstruments) {
        return;
    }

    state_[instrument].phase = 0;
    state_[instrument].pitch_env_phase = 0;
    state_[instrument].amp_env_phase = 0;

    state_[instrument].pitch_env_increment = scaleFirmwareIncrement(
        Res::lut_res_drm_env_increments[patch_[instrument].pitch_decay],
        controlRateScale_);
    state_[instrument].amp_env_increment = scaleFirmwareIncrement(
        Res::lut_res_drm_env_increments[patch_[instrument].amp_decay],
        controlRateScale_);
    state_[instrument].level = u8u8MulShift8(level, patch_[instrument].level);
    playing_ = true;
}

DrumPatch DrumSynth::morphedPatch(uint8_t instrument, uint8_t value) {
    DrumPatch patch;
    if (instrument >= kNumDrumInstruments) {
        return patch;
    }

    const uint8_t offset = static_cast<uint8_t>(instrument * 5 + (value >> 6));
    const uint8_t balance = static_cast<uint8_t>(value << 2);
    const uint8_t* a = kDrumPresets[offset];
    const uint8_t* b = kDrumPresets[offset + 1];
    patch.pitch = u8Mix(a[0], b[0], balance);
    patch.pitch_decay = u8Mix(a[1], b[1], balance);
    patch.pitch_mod = u8Mix(a[2], b[2], balance);
    patch.amp_decay = u8Mix(a[3], b[3], balance);
    patch.crunchiness = u8Mix(a[4], b[4], balance);
    return patch;
}

void DrumSynth::morphPatch(uint8_t instrument, uint8_t value) {
    if (instrument >= kNumDrumInstruments) {
        return;
    }
    const uint8_t level = patch_[instrument].level;
    patch_[instrument] = morphedPatch(instrument, value);
    patch_[instrument].level = level;
}

void DrumSynth::setEnginePatch(uint8_t instrument,
                              uint8_t pitch,
                              uint8_t pitchDecay,
                              uint8_t pitchMod,
                              uint8_t ampDecay,
                              uint8_t crunchiness) {
    if (instrument >= kNumDrumInstruments) {
        return;
    }
    patch_[instrument].pitch = pitch;
    patch_[instrument].pitch_decay = pitchDecay;
    patch_[instrument].pitch_mod = pitchMod;
    patch_[instrument].amp_decay = ampDecay;
    patch_[instrument].crunchiness = crunchiness;
}

void DrumSynth::setParameterCc(uint8_t cc, uint8_t value) {
    if (cc < 16 || cc > 30) {
        return;
    }
    const uint8_t address = kDrumsCcMap[cc - 16];
    auto* data = reinterpret_cast<uint8_t*>(patch_);
    data[address] = static_cast<uint8_t>(value << 1);
}

void DrumSynth::setBalance(uint8_t mix) {
    if (mix < 128) {
        patch_[0].level = 255;
        patch_[1].level = static_cast<uint8_t>(mix << 1);
    } else {
        patch_[0].level = static_cast<uint8_t>(~((mix - 128) << 1));
        patch_[1].level = 255;
    }
    patch_[2].level = patch_[1].level >> 1;
}

void DrumSynth::setBandwidth(uint8_t bandwidth) {
    bandwidth = static_cast<uint8_t>(~bandwidth);
    sampleRateHold_ = static_cast<uint8_t>(bandwidth >> 3);
    updateBitcrushPeriod();
}

void DrumSynth::setVoiceGains(float kick, float snare, float hat) {
    auto sanitize = [](float g) {
        if (!std::isfinite(g) || g < 0.0f) return 0.0f;
        if (g > 4.0f) return 4.0f;
        return g;
    };
    voiceGain_[0] = sanitize(kick);
    voiceGain_[1] = sanitize(snare);
    voiceGain_[2] = sanitize(hat);
}

void DrumSynth::updateModulations() {
    playing_ = false;

    for (uint8_t i = 0; i < kNumDrumInstruments; ++i) {
        const uint32_t prevAmp = state_[i].amp_env_phase;
        state_[i].amp_env_phase += state_[i].amp_env_increment;
        if (state_[i].amp_env_phase < prevAmp) {
            state_[i].amp_env_phase = 0xffffffffu;
            state_[i].amp_env_increment = 0;
        }
        state_[i].amp_level = u8u8MulShift8(
            state_[i].level,
            interpolateSample(Res::wav_res_drm_envelope.data(),
                              static_cast<uint16_t>(state_[i].amp_env_phase >> 16)));

        const uint32_t prevPitchEnv = state_[i].pitch_env_phase;
        state_[i].pitch_env_phase += state_[i].pitch_env_increment;
        if (state_[i].pitch_env_phase < prevPitchEnv) {
            state_[i].pitch_env_phase = 0xffffffffu;
            state_[i].pitch_env_increment = 0;
        }

        uint16_t pitch = static_cast<uint16_t>(patch_[i].pitch) << 8;
        if (i == 0) {
            rngState_ = static_cast<uint16_t>((rngState_ >> 1) ^
                                                (-(rngState_ & 1) & 0xb400));
            const uint8_t randomByte = static_cast<uint8_t>(rngState_ >> 8);
            pitch = static_cast<uint16_t>(pitch + u8u8Mul(randomByte, patch_[i].crunchiness));
        }
        pitch = static_cast<uint16_t>(
            pitch + u8u8Mul(
                        patch_[i].pitch_mod,
                        interpolateSample(Res::wav_res_drm_envelope.data(),
                                          static_cast<uint16_t>(state_[i].pitch_env_phase >> 16))));
        uint16_t firmwareInc = interpolateIncreasing(
            Res::lut_res_drm_phase_increments.data(), pitch);
        if (i == 2)
            firmwareInc >>= 6;
        state_[i].phase_increment = scaleFirmwareIncrement(firmwareInc, audioRateScale_);

        if (state_[i].amp_env_increment != 0) {
            playing_ = true;
        }
    }

    state_[1].amp_level_noise = u8u8MulShift8(state_[1].amp_level, patch_[1].crunchiness);
    state_[1].amp_level = u8u8MulShift8(state_[1].amp_level,
                                          static_cast<uint8_t>(~patch_[1].crunchiness));
}

void DrumSynth::fillWithSilence(size_t numFrames) {
    for (size_t i = 0; i < numFrames; ++i) {
        if (sample_ != 0) {
            if (fadeCounter_ != 0) {
                --fadeCounter_;
            } else {
                fadeCounter_ = 255;
                --sample_;
            }
        }
        if (sampleCounter_ >= bitcrushPeriod_) {
            sampleCounter_ = 0;
        } else {
            ++sampleCounter_;
        }
    }
}

void DrumSynth::processBlock(float* output, size_t numFrames) {
    size_t frame = 0;
    while (frame < numFrames) {
        updateModulations();

        uint32_t phase0 = state_[0].phase;
        uint32_t phase1 = state_[1].phase;
        uint32_t phase2 = state_[2].phase;
        noiseState_ = static_cast<uint8_t>(rngState_ >> 8);

        const size_t blockEnd = std::min(frame + static_cast<size_t>(kAudioBlockSize), numFrames);
        for (; frame < blockEnd; ++frame) {
            ++sampleCounter_;
            int16_t mix = 128;
            noiseState_ = static_cast<uint8_t>((noiseState_ * 73u) + 1u);

            phase0 += state_[0].phase_increment;
            phase1 += state_[1].phase_increment;
            phase2 += state_[2].phase_increment;

            const uint16_t p0 = static_cast<uint16_t>(phase0 >> 16);
            const uint16_t p1 = static_cast<uint16_t>(phase1 >> 16);
            const uint16_t p2 = static_cast<uint16_t>(phase2 >> 16);

            const uint16_t sineIndex = p0 >> 8;
            int8_t bd = static_cast<int8_t>(Res::wav_res_sine[sineIndex]);
            const int8_t bd2 = static_cast<int8_t>(Res::wav_res_sine[sineIndex + 1]);
            bd = static_cast<int8_t>(bd + s8u8MulShift8(static_cast<int8_t>(bd2 - bd),
                                                        static_cast<uint8_t>(p0 & 0xff)));
            const float bdContrib = static_cast<float>(s8u8MulShift8(bd, state_[0].amp_level))
                * voiceGain_[0];
            mix = static_cast<int16_t>(mix + static_cast<int>(bdContrib + (bdContrib >= 0.f ? 0.5f : -0.5f)));

            const int8_t sd = static_cast<int8_t>(Res::wav_res_sine[p1 >> 8]);
            const float sdContrib = static_cast<float>(s8u8MulShift8(sd, state_[1].amp_level))
                * voiceGain_[1];
            mix = static_cast<int16_t>(mix + static_cast<int>(sdContrib + (sdContrib >= 0.f ? 0.5f : -0.5f)));
            const float nzContrib = static_cast<float>(
                s8u8MulShift8(static_cast<int8_t>(noiseState_), state_[1].amp_level_noise))
                * voiceGain_[1];
            mix = static_cast<int16_t>(mix + static_cast<int>(nzContrib + (nzContrib >= 0.f ? 0.5f : -0.5f)));

            const int8_t hh = static_cast<int8_t>(Res::wav_res_hh[p2 >> 4]);
            const float hhContrib = static_cast<float>(s8u8MulShift8(hh, state_[2].amp_level))
                * voiceGain_[2];
            mix = static_cast<int16_t>(mix + static_cast<int>(hhContrib + (hhContrib >= 0.f ? 0.5f : -0.5f)));

            if (sampleCounter_ >= bitcrushPeriod_) {
                if (mix > 255) {
                    mix = 255;
                }
                if (mix < 0) {
                    mix = 0;
                }
                sample_ = static_cast<uint8_t>(mix);
                sampleCounter_ = 0;
            }

            output[frame] = (static_cast<float>(sample_) - 128.0f) / 128.0f;
        }

        state_[0].phase = phase0;
        state_[1].phase = phase1;
        state_[2].phase = phase2;
        fadeCounter_ = 255;
    }
}

}  // namespace dsp
}  // namespace lxx
