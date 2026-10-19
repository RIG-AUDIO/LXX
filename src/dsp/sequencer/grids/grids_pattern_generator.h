// Copyright 2011 Emilie Gillet.
// Host-oriented port for LXX (RIG-AUDIO) — GPLv3 or later.
// Derived from Mutable Instruments Grids pattern_generator (AVR).
#pragma once

#include <array>
#include <cstdint>

#include "sequencer/grids/grids_resources.h"

namespace lxx {
namespace dsp {
namespace grids {

inline uint8_t u8Mix(uint8_t a, uint8_t b, uint8_t mix) noexcept
{
    if (b > a)
        return static_cast<uint8_t>(a + ((static_cast<uint16_t>(b - a) * mix) >> 8));
    return static_cast<uint8_t>(a - ((static_cast<uint16_t>(a - b) * mix) >> 8));
}

inline uint8_t u8u8MulShift8(uint8_t a, uint8_t b) noexcept
{
    return static_cast<uint8_t>((static_cast<uint16_t>(a) * static_cast<uint16_t>(b)) >> 8);
}

struct GridsSettings {
    uint8_t x = 128;
    uint8_t y = 128;
    uint8_t randomness = 0;
    std::array<uint8_t, kNumParts> density { 128, 128, 128 };
};

struct GridsStepResult {
    uint8_t triggers = 0; // bit0 BD, bit1 SD, bit2 HH
    uint8_t accents = 0;  // same bit layout
    uint8_t step = 0;
};

// Instance-based PatternGenerator (original used static AVR state).
class PatternGenerator {
public:
    void reset() noexcept
    {
        step_ = 0;
        perturbation_.fill(0);
        rng_ = 0xA5A5A5A5u;
    }

    void setSeed(uint32_t seed) noexcept
    {
        rng_ = seed == 0u ? 0xA5A5A5A5u : seed;
    }

    void setSettings(const GridsSettings& s) noexcept { settings_ = s; }

    uint8_t currentStep() const noexcept { return step_; }

    GridsStepResult evaluateStep() noexcept
    {
        GridsStepResult out;
        out.step = step_;

        if (step_ == 0) {
            for (uint8_t i = 0; i < kNumParts; ++i) {
                const uint8_t randomness = static_cast<uint8_t>(settings_.randomness >> 2);
                perturbation_[i] = u8u8MulShift8(nextByte(), randomness);
            }
        }

        uint8_t instrumentMask = 1;
        for (uint8_t i = 0; i < kNumParts; ++i) {
            uint8_t level = readDrumMap(step_, i, settings_.x, settings_.y);
            if (level < static_cast<uint8_t>(255u - perturbation_[i]))
                level = static_cast<uint8_t>(level + perturbation_[i]);
            else
                level = 255;

            const uint8_t threshold = static_cast<uint8_t>(~settings_.density[i]);
            if (level > threshold) {
                out.triggers = static_cast<uint8_t>(out.triggers | instrumentMask);
                if (level > 192)
                    out.accents = static_cast<uint8_t>(out.accents | instrumentMask);
            }
            instrumentMask = static_cast<uint8_t>(instrumentMask << 1);
        }

        step_ = static_cast<uint8_t>((step_ + 1u) % kStepsPerPattern);
        return out;
    }

private:
    uint8_t nextByte() noexcept
    {
        rng_ = rng_ * 1664525u + 1013904223u;
        return static_cast<uint8_t>((rng_ >> 24) & 0xffu);
    }

    static uint8_t readDrumMap(uint8_t step, uint8_t instrument, uint8_t x, uint8_t y) noexcept
    {
        const uint8_t i = static_cast<uint8_t>(x >> 6);
        const uint8_t j = static_cast<uint8_t>(y >> 6);
        const uint8_t* aMap = drum_map[i][j];
        const uint8_t* bMap = drum_map[i + 1][j];
        const uint8_t* cMap = drum_map[i][j + 1];
        const uint8_t* dMap = drum_map[i + 1][j + 1];
        const uint8_t offset = static_cast<uint8_t>((instrument * kStepsPerPattern) + step);
        const uint8_t a = aMap[offset];
        const uint8_t b = bMap[offset];
        const uint8_t c = cMap[offset];
        const uint8_t d = dMap[offset];
        return u8Mix(u8Mix(a, b, static_cast<uint8_t>(x << 2)),
                     u8Mix(c, d, static_cast<uint8_t>(x << 2)),
                     static_cast<uint8_t>(y << 2));
    }

    GridsSettings settings_ {};
    uint8_t step_ = 0;
    std::array<uint8_t, kNumParts> perturbation_ {};
    uint32_t rng_ = 0xA5A5A5A5u;
};

}  // namespace grids
}  // namespace dsp
}  // namespace lxx
