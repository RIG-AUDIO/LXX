#include "oversampled_filter.h"

#include <algorithm>

namespace lxx {
namespace dsp {

OversampledSVF::OversampledSVF(Sample baseSampleRate, Quality quality)
    : baseSampleRate_(baseSampleRate),
      quality_(quality),
      filter_(baseSampleRate) {
    oversampleRatio_ = static_cast<int>(quality_);
    setQuality(quality);
}

void OversampledSVF::setSampleRate(Sample baseSampleRate) {
    baseSampleRate_ = baseSampleRate;
    filter_.setSampleRate(baseSampleRate_ * oversampleRatio_);
}

void OversampledSVF::setQuality(Quality quality) {
    quality_ = quality;
    oversampleRatio_ = static_cast<int>(quality_);

    Oversampler::Factor factor = (quality_ == Quality::High)
        ? Oversampler::Factor::x4
        : Oversampler::Factor::x2;

    upsampler_.setFactor(factor);
    downsamplerLp_.setFactor(factor);
    downsamplerBp_.setFactor(factor);
    downsamplerHp_.setFactor(factor);

    const size_t maxBlockSize = 2048;
    prepareBuffers(maxBlockSize);
    filter_.setSampleRate(baseSampleRate_ * oversampleRatio_);
}

void OversampledSVF::prepareBuffers(size_t maxBaseFrames) {
    const size_t frames = maxBaseFrames > 0 ? maxBaseFrames : 2048;
    const size_t upFrames = frames * static_cast<size_t>(oversampleRatio_ > 0 ? oversampleRatio_ : 2);
    upsampler_.prepare(frames);
    downsamplerLp_.prepare(frames);
    downsamplerBp_.prepare(frames);
    downsamplerHp_.prepare(frames);
    upBuffer_.assign(upFrames, 0.0f);
    lpBuffer_.assign(upFrames, 0.0f);
    bpBuffer_.assign(upFrames, 0.0f);
    hpBuffer_.assign(upFrames, 0.0f);
}

void OversampledSVF::setCutoffFreq(Sample freqHz) {
    filter_.setCutoffFreq(freqHz);
}

void OversampledSVF::setCutoffCV(Sample cv_dac, Sample cv_input, Sample cv_pot) {
    filter_.setCutoffCV(cv_dac, cv_input, cv_pot);
}

void OversampledSVF::setResonance(Sample value) {
    filter_.setResonance(value);
}

void OversampledSVF::setResonanceCV(Sample cv) {
    filter_.setResonanceCV(cv);
}

void OversampledSVF::setUseAnuResonance(bool useAnu) {
    filter_.setUseAnuResonance(useAnu);
}

void OversampledSVF::setAuthenticMode(bool enabled) {
    filter_.setAuthenticMode(enabled);
}

void OversampledSVF::setNonlinearCalibration(const SVFNonlinearCalibration& cal) {
    filter_.setCalibration(cal);
}

std::array<Sample, 3> OversampledSVF::process(Sample input) {
    Sample up[4];
    upsampler_.upsample(&input, 1, up);

    Sample lp[4], bp[4], hp[4];
    filter_.processBlock(up, lp, bp, hp, oversampleRatio_);

    Sample lpOut, bpOut, hpOut;
    downsamplerLp_.downsample(lp, 1, &lpOut);
    downsamplerBp_.downsample(bp, 1, &bpOut);
    downsamplerHp_.downsample(hp, 1, &hpOut);

    return { lpOut, bpOut, hpOut };
}

void OversampledSVF::processBlock(const Sample* input, Sample* lpOut, Sample* bpOut, Sample* hpOut, size_t numFrames) {
    const size_t maxBase = oversampleRatio_ > 0 ? (upBuffer_.size() / static_cast<size_t>(oversampleRatio_)) : 0;
    if (maxBase == 0 || input == nullptr || lpOut == nullptr || bpOut == nullptr || hpOut == nullptr) {
        if (lpOut != nullptr && bpOut != nullptr && hpOut != nullptr) {
            for (size_t i = 0; i < numFrames; ++i) {
                lpOut[i] = 0.0f;
                bpOut[i] = 0.0f;
                hpOut[i] = 0.0f;
            }
        }
        return;
    }

    size_t offset = 0;
    while (offset < numFrames) {
        const size_t chunk = std::min(numFrames - offset, maxBase);
        if (chunk == 0)
            break;
        const size_t upFrames = chunk * static_cast<size_t>(oversampleRatio_);
        upsampler_.upsample(input + offset, chunk, upBuffer_.data());
        filter_.processBlock(upBuffer_.data(), lpBuffer_.data(), bpBuffer_.data(), hpBuffer_.data(), upFrames);
        downsamplerLp_.downsample(lpBuffer_.data(), chunk, lpOut + offset);
        downsamplerBp_.downsample(bpBuffer_.data(), chunk, bpOut + offset);
        downsamplerHp_.downsample(hpBuffer_.data(), chunk, hpOut + offset);
        offset += chunk;
    }
}

void OversampledSVF::reset() {
    upsampler_.reset();
    downsamplerLp_.reset();
    downsamplerBp_.reset();
    downsamplerHp_.reset();
    filter_.reset();
}

} // namespace dsp
} // namespace lxx
