#include "oversampler.h"

#include <algorithm>

namespace lxx {
namespace dsp {

Oversampler::Oversampler(Factor factor, Quality quality) {
    setFactor(factor);
    setQuality(quality);
    reset();
}

void Oversampler::reset() {
    resetStage(stage1_);
    resetStage(stage2_);
}

void Oversampler::resetStage(StageState& stage) {
    stage.upDelay.fill(0.0f);
    stage.downDelay.fill(0.0f);
    stage.downCounter = 0;
}

void Oversampler::setFactor(Factor factor) {
    factor_ = factor;
    oversampleRatio_ = (factor == Factor::x4) ? 4 : 2;
}

void Oversampler::setQuality(Quality quality) {
    quality_ = quality;
    if (quality == Quality::High) {
        factor_ = Factor::x4;
        oversampleRatio_ = 4;
    } else {
        factor_ = Factor::x2;
        oversampleRatio_ = 2;
    }
}

void Oversampler::prepare(size_t maxBaseFrames) {
    const size_t n = maxBaseFrames > 0 ? maxBaseFrames : 2048;
    const size_t ratio = static_cast<size_t>(oversampleRatio_ > 0 ? oversampleRatio_ : 4);
    x4Temp_.assign(n * 2, 0.0f);
    processTemp_.assign(n * ratio, 0.0f);
}

Sample Oversampler::processSample(Sample input) {
    Sample output = 0.0f;
    processBlock(&input, &output, 1);
    return output;
}

void Oversampler::processBlock(const Sample* input, Sample* output, size_t numFrames) {
    if (input == nullptr || output == nullptr || numFrames == 0)
        return;

    const size_t ratio = static_cast<size_t>(oversampleRatio_ > 0 ? oversampleRatio_ : 2);
    if (processTemp_.empty()) {
        for (size_t i = 0; i < numFrames; ++i)
            output[i] = 0.0f;
        return;
    }
    size_t offset = 0;
    while (offset < numFrames) {
        const size_t maxBase = std::max<size_t>(1, processTemp_.size() / ratio);
        const size_t chunk = std::min(numFrames - offset, maxBase);
        if (chunk == 0)
            break;
        upsample(input + offset, chunk, processTemp_.data());
        downsample(processTemp_.data(), chunk, output + offset);
        offset += chunk;
    }
}

Oversampler::OversampledBuffers Oversampler::prepareBuffers(size_t baseFrames) {
    const size_t upFrames = baseFrames * oversampleRatio_;
    return { std::vector<Sample>(upFrames), std::vector<Sample>(upFrames) };
}

void Oversampler::upsampleStage(const Sample* input, size_t numFrames, Sample* upBuffer, StageState& stage) {
    for (size_t i = 0; i < numFrames; ++i) {
        for (int j = kEvenLen - 1; j > 0; --j) {
            stage.upDelay[j] = stage.upDelay[j - 1];
        }
        stage.upDelay[0] = input[i];

        Sample evenOut = 0.0f;
        for (int j = 0; j < kEvenLen; ++j) {
            evenOut += kHalfBandFir[j * 2] * stage.upDelay[j];
        }

        Sample oddOut = 0.0f;
        for (int j = 0; j < kOddLen; ++j) {
            oddOut += kHalfBandFir[j * 2 + 1] * stage.upDelay[j];
        }

        upBuffer[i * 2] = evenOut;
        upBuffer[i * 2 + 1] = oddOut;
    }
}

void Oversampler::upsample(const Sample* input, size_t numFrames, Sample* upBuffer) {
    if (factor_ == Factor::x4) {
        const size_t need = numFrames * 2;
        if (x4Temp_.size() < need) {
            return;
        }
        upsampleStage(input, numFrames, x4Temp_.data(), stage1_);
        upsampleStage(x4Temp_.data(), numFrames * 2, upBuffer, stage2_);
    } else {
        upsampleStage(input, numFrames, upBuffer, stage1_);
    }
}

void Oversampler::downsampleStage(const Sample* upBuffer, size_t numHighSamples, Sample* output, StageState& stage) {
    size_t outIdx = 0;
    const size_t numOutputs = numHighSamples / 2;

    for (size_t i = 0; i < numHighSamples; ++i) {
        for (int j = kFirLen - 1; j > 0; --j) {
            stage.downDelay[j] = stage.downDelay[j - 1];
        }
        stage.downDelay[0] = upBuffer[i];

        stage.downCounter++;
        if (stage.downCounter % 2 == 0 && outIdx < numOutputs) {
            Sample sum = 0.0f;
            for (int j = 0; j < kFirLen; ++j) {
                sum += kHalfBandFir[j] * stage.downDelay[j];
            }
            output[outIdx++] = sum;
        }
    }
}

void Oversampler::downsample(const Sample* upBuffer, size_t numFrames, Sample* output) {
    if (factor_ == Factor::x4) {
        const size_t need = numFrames * 2;
        if (x4Temp_.size() < need) {
            return;
        }
        downsampleStage(upBuffer, numFrames * 4, x4Temp_.data(), stage2_);
        downsampleStage(x4Temp_.data(), numFrames * 2, output, stage1_);
    } else {
        downsampleStage(upBuffer, numFrames * 2, output, stage1_);
    }
}

} // namespace dsp
} // namespace lxx
