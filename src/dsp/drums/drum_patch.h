#pragma once

#include <cstdint>

namespace lxx {
namespace dsp {

constexpr int kNumDrumInstruments = 3;
constexpr int kAudioBlockSize = 32;

// Firmware audio / control rates (20000000 / 510 Hz, block size 32).
constexpr float kFirmwareAudioRate = 20000000.0f / 510.0f;
constexpr float kFirmwareDrumControlRate = kFirmwareAudioRate / static_cast<float>(kAudioBlockSize);

struct DrumPatch {
    uint8_t pitch = 60;
    uint8_t pitch_decay = 18;
    uint8_t pitch_mod = 104;
    uint8_t amp_decay = 120;
    uint8_t crunchiness = 0;
    uint8_t level = 255;
};

struct DrumState {
    // 16.16 fixed-point: top 16 bits match the original firmware uint16 phase.
    // Extra fraction bits keep tiny HH increments accurate at high host rates.
    uint32_t phase = 0;
    uint32_t phase_increment = 0;
    uint32_t pitch_env_phase = 0;
    uint32_t pitch_env_increment = 0;
    uint32_t amp_env_phase = 0;
    uint32_t amp_env_increment = 0;
    uint8_t amp_level = 0;
    uint8_t amp_level_noise = 0;
    uint8_t level = 0;
};

}  // namespace dsp
}  // namespace lxx
