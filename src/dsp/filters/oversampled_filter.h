#pragma once

#include "svf_anushri.h"
#include "svf_anushri_nonlinear.h"
#include "../core/oversampler.h"
#include <array>
#include <vector>

namespace lxx {
namespace dsp {

/**
 * Oversampled Filter Wrapper
 *
 * Base rate in -> upsample -> NL SVF at 2x/4x -> downsample -> base rate out.
 * Separate downsampler state per output (LP/BP/HP).
 */

class OversampledSVF {
public:
    enum class Quality { Standard = 2, High = 4 };

    OversampledSVF(Sample baseSampleRate = kSampleRate, Quality quality = Quality::Standard);

    void setSampleRate(Sample baseSampleRate);
    void setQuality(Quality quality);

    void setCutoffFreq(Sample freqHz);
    void setCutoffCV(Sample cv_dac, Sample cv_input, Sample cv_pot);
    void setResonance(Sample value);
    void setResonanceCV(Sample cv);
    void setUseAnuResonance(bool useAnu);
    void setAuthenticMode(bool enabled);
    void setNonlinearCalibration(const SVFNonlinearCalibration& cal);

    std::array<Sample, 3> process(Sample input);
    void processBlock(const Sample* input, Sample* lpOut, Sample* bpOut, Sample* hpOut, size_t numFrames);

    SVFAnushriNonlinear& getFilter() { return filter_; }
    const SVFAnushriNonlinear& getFilter() const { return filter_; }

    Sample getEffectiveSampleRate() const { return baseSampleRate_ * oversampleRatio_; }

    void prepareBuffers(size_t maxBaseFrames);
    void reset();

private:
    Sample baseSampleRate_;
    Quality quality_;
    int oversampleRatio_;

    Oversampler upsampler_;
    Oversampler downsamplerLp_;
    Oversampler downsamplerBp_;
    Oversampler downsamplerHp_;

    SVFAnushriNonlinear filter_;

    std::vector<Sample> upBuffer_;
    std::vector<Sample> lpBuffer_;
    std::vector<Sample> bpBuffer_;
    std::vector<Sample> hpBuffer_;
};

} // namespace dsp
} // namespace lxx
