#pragma once

#include "drums/drum_patch.h"
#include "drums/drum_resources.h"
#include "engines/drum_events.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace lxx {
namespace dsp {

/**
 * Native LXX extended voices sharing DrumSynth vocabulary:
 * phase oscillators, firmware LUT envelopes, sine/HH tables, LFSR-ish noise.
 * Voices: LOW (Boom/Tom articulations), Mid Perc, Open Hat.
 */
class NativeExtendedVoices {
public:
    struct Params {
        uint8_t lowPitch = 48;
        uint8_t lowPitchDecay = 40;
        uint8_t lowPitchMod = 120;
        uint8_t lowAmpDecay = 160;
        uint8_t lowTone = 128;
        float lowCharacter = 0.0f; // 0 boom .. 1 tom (forced region when not auto)
        int lowMode = 0;           // 0 auto, 1 boom, 2 tom
        float lowLevel = 0.55f;

        uint8_t midPitch = 100;
        uint8_t midPitchDecay = 28;
        uint8_t midPitchMod = 90;
        uint8_t midAmpDecay = 100;
        uint8_t midTone = 140;
        float midLevel = 0.42f;

        uint8_t ohPitch = 148;
        uint8_t ohAmpDecay = 40; // smaller LUT index → longer open
        uint8_t ohTone = 128;
        float ohLevel = 0.32f;
    };

    void init(float sampleRate)
    {
        sampleRate_ = sampleRate > 1.0f && std::isfinite(sampleRate) ? sampleRate : 48000.0f;
        reset();
        updateRateScaling();
    }

    void reset()
    {
        std::memset(state_, 0, sizeof(state_));
        ohScale_ = 1.0f;
        noise_ = 0;
        playing_ = false;
    }

    void setParams(const Params& p) { params_ = p; }
    const Params& params() const noexcept { return params_; }

    /** Linear gains for LOW / Mid / OH (default 1). Mute = 0. */
    void setVoiceGains(float low, float mid, float oh)
    {
        auto sanitize = [](float g) {
            if (!std::isfinite(g) || g < 0.0f) return 0.0f;
            if (g > 4.0f) return 4.0f;
            return g;
        };
        gainLow_ = sanitize(low);
        gainMid_ = sanitize(mid);
        gainOH_ = sanitize(oh);
    }

    void triggerLow(float velocity01, LowArticulation art)
    {
        const float v = clamp01(velocity01);
        activeArt_ = art;

        DrumPatch patch;
        if (art == LowArticulation::boom) {
            patch.pitch = minU8(params_.lowPitch, 56);
            patch.pitch_decay = params_.lowPitchDecay;
            patch.pitch_mod = maxU8(params_.lowPitchMod, 100);
            patch.amp_decay = maxU8(params_.lowAmpDecay, 140);
            patch.crunchiness = static_cast<uint8_t>(params_.lowTone >> 3);
        } else {
            const int pitched = static_cast<int>(params_.lowPitch) + 18
                + static_cast<int>(params_.lowCharacter * 24.0f);
            patch.pitch = static_cast<uint8_t>(clampInt(pitched, 40, 100));
            patch.pitch_decay = minU8(params_.lowPitchDecay, 36);
            patch.pitch_mod = params_.lowPitchMod;
            patch.amp_decay = minU8(params_.lowAmpDecay, 120);
            patch.crunchiness = 0;
        }
        patch.level = 255;
        startVoice(kLow, patch, v * params_.lowLevel);
    }

    void triggerMid(float velocity01)
    {
        const float v = clamp01(velocity01);
        DrumPatch patch;
        patch.pitch = params_.midPitch;
        patch.pitch_decay = params_.midPitchDecay;
        patch.pitch_mod = params_.midPitchMod;
        patch.amp_decay = params_.midAmpDecay;
        patch.crunchiness = static_cast<uint8_t>(params_.midTone >> 1);
        patch.level = 255;
        startVoice(kMid, patch, v * params_.midLevel);
    }

    void triggerOpenHat(float velocity01)
    {
        const float v = clamp01(velocity01);
        ohScale_ = 1.0f;
        DrumPatch patch;
        patch.pitch = params_.ohPitch;
        patch.pitch_decay = 0;
        patch.pitch_mod = 0;
        patch.amp_decay = params_.ohAmpDecay;
        patch.crunchiness = 0;
        patch.level = 255;
        startVoice(kOH, patch, v * params_.ohLevel);
    }

    void chokeOpenHat()
    {
        ohScale_ = 0.0f;
        state_[kOH].amp_env_increment = 0;
        state_[kOH].amp_level = 0;
    }

    void processAdd(float* out, uint32_t n)
    {
        if (n == 0 || out == nullptr)
            return;

        for (uint32_t i = 0; i < n; ++i) {
            if ((i % kAudioBlockSize) == 0)
                updateModulations();

            float s = 0.0f;

            {
                auto& st = state_[kLow];
                st.phase += st.phase_increment;
                const uint16_t p = static_cast<uint16_t>(st.phase >> 16);
                const uint16_t idx = static_cast<uint16_t>(p >> 8);
                int8_t bd = static_cast<int8_t>(drum_resources::wav_res_sine[idx]);
                const int8_t bd2 = static_cast<int8_t>(drum_resources::wav_res_sine[idx + 1]);
                bd = static_cast<int8_t>(bd + s8Mul(static_cast<int8_t>(bd2 - bd),
                                                     static_cast<uint8_t>(p & 0xff)));
                float body = (static_cast<float>(bd) / 128.0f)
                    * (static_cast<float>(st.amp_level) / 255.0f);
                if (activeArt_ == LowArticulation::boom && st.amp_level_noise > 0) {
                    noise_ = static_cast<uint8_t>(noise_ * 73u + 1u);
                    body += (static_cast<float>(static_cast<int8_t>(noise_)) / 128.0f)
                        * (static_cast<float>(st.amp_level_noise) / 255.0f) * 0.25f;
                }
                s += body * 0.85f * gainLow_;
            }

            {
                auto& st = state_[kMid];
                st.phase += st.phase_increment;
                const uint16_t p = static_cast<uint16_t>(st.phase >> 16);
                const int8_t sd = static_cast<int8_t>(drum_resources::wav_res_sine[p >> 8]);
                noise_ = static_cast<uint8_t>(noise_ * 73u + 1u);
                const float body = (static_cast<float>(sd) / 128.0f)
                    * (static_cast<float>(st.amp_level) / 255.0f);
                const float nz = (static_cast<float>(static_cast<int8_t>(noise_)) / 128.0f)
                    * (static_cast<float>(st.amp_level_noise) / 255.0f);
                s += (body + nz * 0.55f) * 0.7f * gainMid_;
            }

            {
                auto& st = state_[kOH];
                st.phase += st.phase_increment;
                const uint16_t p = static_cast<uint16_t>(st.phase >> 16);
                const int8_t hh = static_cast<int8_t>(drum_resources::wav_res_hh[p >> 4]);
                const float h = (static_cast<float>(hh) / 128.0f)
                    * (static_cast<float>(st.amp_level) / 255.0f);
                s += h * 0.55f * ohScale_ * gainOH_;
            }

            if (!std::isfinite(s))
                s = 0.0f;
            out[i] += s;
        }
    }

    bool isPlaying() const { return playing_ || ohScale_ > 0.0f; }

private:
    static constexpr int kLow = 0;
    static constexpr int kMid = 1;
    static constexpr int kOH = 2;
    static constexpr int kNum = 3;

    static float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
    static uint8_t minU8(uint8_t a, uint8_t b) { return a < b ? a : b; }
    static uint8_t maxU8(uint8_t a, uint8_t b) { return a > b ? a : b; }
    static int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

    static uint8_t u8Mul(uint8_t a, uint8_t b)
    {
        return static_cast<uint8_t>((static_cast<uint16_t>(a) * b) >> 8);
    }
    static int8_t s8Mul(int8_t a, uint8_t b)
    {
        return static_cast<int8_t>((static_cast<int16_t>(a) * b) >> 8);
    }
    static uint8_t interpolateSample(const uint8_t* table, uint16_t phase)
    {
        const uint8_t index = static_cast<uint8_t>(phase >> 8);
        return static_cast<uint8_t>(
            (table[index] * (255 - (phase & 0xff)) + table[index + 1] * (phase & 0xff)) >> 8);
    }
    static uint16_t interpolateIncreasing(const uint16_t* table, uint16_t phase)
    {
        const uint16_t index = phase >> 8;
        const uint16_t a = table[index];
        const uint16_t b = table[index + 1];
        return static_cast<uint16_t>(
            a + u8Mul(static_cast<uint8_t>(b - a), static_cast<uint8_t>(phase & 0xff)));
    }
    static uint32_t scaleFirmwareIncrement(uint16_t increment, float scale)
    {
        const double scaled = static_cast<double>(increment) * static_cast<double>(scale) * 65536.0;
        if (!(scaled > 0.0))
            return 0;
        if (scaled >= 4294967295.0)
            return 0xffffffffu;
        return static_cast<uint32_t>(scaled + 0.5);
    }

    void updateRateScaling()
    {
        const float sr = sampleRate_;
        audioRateScale_ = kFirmwareAudioRate / sr;
        const float controlRate = sr / static_cast<float>(kAudioBlockSize);
        controlRateScale_ = kFirmwareDrumControlRate / controlRate;
    }

    void startVoice(int idx, const DrumPatch& patch, float level01)
    {
        patch_[idx] = patch;
        auto& st = state_[idx];
        st.phase = 0;
        st.pitch_env_phase = 0;
        st.amp_env_phase = 0;
        st.pitch_env_increment = scaleFirmwareIncrement(
            drum_resources::lut_res_drm_env_increments[patch.pitch_decay], controlRateScale_);
        st.amp_env_increment = scaleFirmwareIncrement(
            drum_resources::lut_res_drm_env_increments[patch.amp_decay], controlRateScale_);
        int lvl = static_cast<int>(level01 * 255.0f + 0.5f);
        if (lvl < 1)
            lvl = 1;
        if (lvl > 255)
            lvl = 255;
        st.level = u8Mul(static_cast<uint8_t>(lvl), patch.level);
        playing_ = true;
    }

    void updateModulations()
    {
        playing_ = false;
        for (int i = 0; i < kNum; ++i) {
            auto& st = state_[i];
            const uint32_t prevAmp = st.amp_env_phase;
            st.amp_env_phase += st.amp_env_increment;
            if (st.amp_env_phase < prevAmp) {
                st.amp_env_phase = 0xffffffffu;
                st.amp_env_increment = 0;
            }
            st.amp_level = u8Mul(st.level,
                interpolateSample(drum_resources::wav_res_drm_envelope.data(),
                                  static_cast<uint16_t>(st.amp_env_phase >> 16)));

            const uint32_t prevPitch = st.pitch_env_phase;
            st.pitch_env_phase += st.pitch_env_increment;
            if (st.pitch_env_phase < prevPitch) {
                st.pitch_env_phase = 0xffffffffu;
                st.pitch_env_increment = 0;
            }

            uint16_t pitch = static_cast<uint16_t>(patch_[i].pitch) << 8;
            pitch = static_cast<uint16_t>(
                pitch + static_cast<uint16_t>(patch_[i].pitch_mod)
                    * interpolateSample(drum_resources::wav_res_drm_envelope.data(),
                                        static_cast<uint16_t>(st.pitch_env_phase >> 16))
                    / 255u);

            uint16_t firmwareInc = interpolateIncreasing(
                drum_resources::lut_res_drm_phase_increments.data(), pitch);
            if (i == kOH)
                firmwareInc >>= 6;
            st.phase_increment = scaleFirmwareIncrement(firmwareInc, audioRateScale_);

            if (i == kMid) {
                st.amp_level_noise = u8Mul(st.amp_level, patch_[i].crunchiness);
                st.amp_level = u8Mul(st.amp_level, static_cast<uint8_t>(~patch_[i].crunchiness));
            } else if (i == kLow && activeArt_ == LowArticulation::boom) {
                st.amp_level_noise = u8Mul(st.amp_level, patch_[i].crunchiness);
            } else {
                st.amp_level_noise = 0;
            }

            if (st.amp_env_increment != 0)
                playing_ = true;
        }
        if (ohScale_ > 0.0f && state_[kOH].amp_env_increment == 0) {
            ohScale_ *= 0.995f;
            if (ohScale_ < 1.0e-4f || state_[kOH].amp_level == 0)
                ohScale_ = 0.0f;
        }
    }

    float sampleRate_ = 48000.0f;
    float audioRateScale_ = 1.0f;
    float controlRateScale_ = 1.0f;
    Params params_ {};
    DrumPatch patch_[kNum] {};
    DrumState state_[kNum] {};
    LowArticulation activeArt_ = LowArticulation::boom;
    float ohScale_ = 1.0f;
    float gainLow_ = 1.0f;
    float gainMid_ = 1.0f;
    float gainOH_ = 1.0f;
    uint8_t noise_ = 0;
    bool playing_ = false;
};

}  // namespace dsp
}  // namespace lxx
