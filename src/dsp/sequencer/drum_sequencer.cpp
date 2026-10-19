#include "sequencer/drum_sequencer.h"

#include "sequencer/drum_map_resources.h"

namespace lxx {
namespace dsp {

uint8_t DrumSequencer::u8Mix(uint8_t a, uint8_t b, uint8_t mix)
{
    if (b > a) {
        return static_cast<uint8_t>(a + ((static_cast<uint16_t>(b - a) * mix) >> 8));
    }
    return static_cast<uint8_t>(a - ((static_cast<uint16_t>(a - b) * mix) >> 8));
}

uint8_t DrumSequencer::readDrumMap(uint8_t step, uint8_t instrument, uint8_t x, uint8_t y)
{
    const uint8_t i = x >> 7;
    const uint8_t j = y >> 7;

    const auto& aMap = drumMapNode(i, j);
    const auto& bMap = drumMapNode(static_cast<uint8_t>(i + 1), j);
    const auto& cMap = drumMapNode(i, static_cast<uint8_t>(j + 1));
    const auto& dMap = drumMapNode(static_cast<uint8_t>(i + 1), static_cast<uint8_t>(j + 1));

    const uint8_t offset = static_cast<uint8_t>(u8ShiftLeft4(instrument) + step);
    if (offset >= kDrumMapNodeBytes) {
        return 0;
    }
    const uint8_t a = aMap[offset];
    const uint8_t b = bMap[offset];
    const uint8_t c = cMap[offset];
    const uint8_t d = dMap[offset];

    return u8Mix(u8Mix(a, b, static_cast<uint8_t>(x << 1)),
                   u8Mix(c, d, static_cast<uint8_t>(x << 1)),
                   static_cast<uint8_t>(y << 1));
}

void DrumSequencer::start()
{
    step_ = 0;
    perturbation_.fill(0);
}

FixedEventList<DrumHitEvent, kNumDrumParts> DrumSequencer::clockStep(
    const SequencerSettings& settings, bool sequencerRunning, uint32_t* rngState)
{
    FixedEventList<DrumHitEvent, kNumDrumParts> hits;
    if (!sequencerRunning || !settings.hasDrums()) {
        return hits;
    }

    const uint16_t stepMask = static_cast<uint16_t>(1u << step_);
    uint8_t overrideMask = 1;
    const uint8_t x = settings.drumsX;
    const uint8_t y = settings.drumsY;

    for (uint8_t i = 0; i < kNumDrumParts; ++i) {
        uint8_t level = readDrumMap(step_, i, x, y);
        if (level < static_cast<uint8_t>(255u - perturbation_[i])) {
            level = static_cast<uint8_t>(level + perturbation_[i]);
        }

        uint8_t threshold = static_cast<uint8_t>(~settings.drumsDensity[i]);

        if ((settings.drumsOverride & overrideMask) != 0) {
            level = (settings.drumsPattern[i] & stepMask) != 0 ? 255 : 0;
            threshold = 0;
        }

        if (level > threshold) {
            DrumHitEvent hit;
            hit.part = i;
            hit.level = static_cast<uint8_t>(128 + (level >> 1));
            hits.push(hit);
        }

        overrideMask = static_cast<uint8_t>(overrideMask << 1);
    }

    ++step_;
    if (step_ >= kDrumSteps) {
        step_ = 0;
        for (uint8_t i = 0; i < kNumDrumParts; ++i) {
            if (rngState != nullptr) {
                *rngState = (*rngState * 1664525u) + 1013904223u;
                perturbation_[i] = static_cast<uint8_t>((*rngState >> 24) >> 3);
            } else {
                perturbation_[i] = static_cast<uint8_t>((i * 37u + step_) & 0x1fu);
            }
        }
    }

    return hits;
}

}  // namespace dsp
}  // namespace lxx
