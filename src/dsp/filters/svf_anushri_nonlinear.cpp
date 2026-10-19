#include "svf_anushri_nonlinear.h"
#include <cmath>

namespace lxx {
namespace dsp {

SVFAnushriNonlinear::SVFAnushriNonlinear(Sample sampleRate) : svfLinear_(sampleRate) {
}

void SVFAnushriNonlinear::setSampleRate(Sample sampleRate) {
    svfLinear_.setSampleRate(sampleRate);
}

void SVFAnushriNonlinear::setCalibration(const SVFNonlinearCalibration& cal) {
    cal_ = cal;
}

void SVFAnushriNonlinear::setCutoffFreq(Sample freqHz) {
    svfLinear_.setCutoffFreq(freqHz);
}

void SVFAnushriNonlinear::setCutoffCV(Sample cv_dac, Sample cv_input, Sample cv_pot) {
    svfLinear_.setCutoffCV(cv_dac, cv_input, cv_pot);
}

void SVFAnushriNonlinear::setResonance(Sample value) {
    svfLinear_.setResonance(value);
}

void SVFAnushriNonlinear::setResonanceCV(Sample cv) {
    svfLinear_.setResonanceCV(cv);
}

void SVFAnushriNonlinear::setUseAnuResonance(bool useAnu) {
    svfLinear_.setUseAnuResonance(useAnu);
}

void SVFAnushriNonlinear::setFilterMode(SVFOutput mode) {
    svfLinear_.setFilterMode(mode);
}

void SVFAnushriNonlinear::setAuthenticMode(bool enabled) {
    svfLinear_.setAuthenticMode(enabled);
    if (enabled) {
        cal_.enableInputSaturation = true;
        cal_.enableIntegratorSaturation = true;
        cal_.enableResonanceNonlinearity = true;
        cal_.inputSaturationThreshold = kOtaInputSaturationThreshold;
        cal_.integratorSaturationThreshold = kOtaSaturationThreshold;
    }
}

std::array<Sample, 3> SVFAnushriNonlinear::process(Sample input) {
    Sample lp, bp, hp;
    processNonlinear(input, lp, bp, hp);
    return { lp, bp, hp };
}

void SVFAnushriNonlinear::processBlock(const Sample* input, Sample* lpOut, Sample* bpOut, Sample* hpOut, size_t numFrames) {
    for (size_t i = 0; i < numFrames; ++i) {
        Sample lp, bp, hp;
        processNonlinear(input[i], lp, bp, hp);
        lpOut[i] = lp;
        bpOut[i] = bp;
        hpOut[i] = hp;
    }
}

inline Sample otasaturate(Sample x, Sample threshold, Sample positiveScale, Sample negativeScale) {
    if (threshold <= 0.0f) {
        return x;
    }
    if (x >= 0.0f) {
        return positiveScale * threshold * std::tanh(x / threshold);
    }
    return negativeScale * threshold * std::tanh(x / threshold);
}

void SVFAnushriNonlinear::processNonlinear(Sample input, Sample& lp, Sample& bp, Sample& hp) {
    const Sample g = svfLinear_.getG();
    const Sample k = svfLinear_.getK();
    const Sample resonance = svfLinear_.getResonance();

    Sample ic2eq = state_[0];
    Sample ic1eq = state_[1];

    Sample satInput = input;
    if (!std::isfinite(satInput) || !std::isfinite(ic1eq) || !std::isfinite(ic2eq)) {
        state_[0] = state_[1] = state_[2] = 0.0f;
        lp = bp = hp = 0.0f;
        return;
    }
    if (cal_.enableInputSaturation) {
        satInput = otasaturate(input, cal_.inputSaturationThreshold,
                               cal_.positiveSaturation, cal_.negativeSaturation);
    }

    const Sample v0 = satInput - ic2eq;

    Sample kEff = k;
    if (cal_.enableResonanceNonlinearity && k > 0.0001f && resonance < 1.0f) {
        const Sample nlFade = clamp(1.0f - resonance, 0.0f, 1.0f);
        const Sample level = std::abs(ic1eq) / cal_.integratorSaturationThreshold;
        const Sample excess = clamp(level, 0.0f, 4.0f);
        kEff = k + nlFade * excess * excess * k * 0.5f;
    }

    const Sample denom = 1.0f + g * (g + kEff);
    const Sample invDen = (std::isfinite(denom) && denom > 1.0e-12f) ? (1.0f / denom) : 0.0f;
    Sample v1 = (ic1eq + g * v0) * invDen;
    ic1eq = 2.0f * v1 - ic1eq;
    const Sample v2 = ic2eq + g * v1;
    ic2eq = 2.0f * v2 - ic2eq;

    lp = v2;
    bp = v1;
    hp = v0 - kEff * v1 - v2;

    if (cal_.enableIntegratorSaturation) {
        const Sample thr = cal_.integratorSaturationThreshold;
        bp = otasaturate(bp, thr, cal_.positiveSaturation, cal_.negativeSaturation);
        lp = otasaturate(lp, thr, cal_.positiveSaturation, cal_.negativeSaturation);
    }

    if (!std::isfinite(ic1eq) || !std::isfinite(ic2eq) || !std::isfinite(lp)
        || !std::isfinite(bp) || !std::isfinite(hp)) {
        state_[0] = state_[1] = state_[2] = 0.0f;
        lp = bp = hp = 0.0f;
        return;
    }

    constexpr Sample kMaxState = SVFAnushriLinear::kMaxIntegrator;
    state_[0] = clamp(ic2eq, -kMaxState, kMaxState);
    state_[1] = clamp(ic1eq, -kMaxState, kMaxState);
    state_[2] = clamp(hp, -kMaxState, kMaxState);
    lp = clamp(lp, -kMaxState, kMaxState);
    bp = clamp(bp, -kMaxState, kMaxState);
    hp = clamp(hp, -kMaxState, kMaxState);
}

void SVFAnushriNonlinear::reset() {
    svfLinear_.reset();
    state_[0] = state_[1] = state_[2] = 0.0f;
}

} // namespace dsp
} // namespace lxx
