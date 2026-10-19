#include "svf_anushri.h"
#include <cmath>

namespace lxx {
namespace dsp {

SVFAnushriLinear::SVFAnushriLinear(Sample sampleRate) : sampleRate_(sampleRate), ssm2164_() {
    reset();
    updateCoefficients();
}

void SVFAnushriLinear::setSampleRate(Sample sampleRate) {
    if (sampleRate == sampleRate_)
        return;
    sampleRate_ = sampleRate;
    updateCoefficients();
}

void SVFAnushriLinear::reset() {
    state_[0] = 0.0f;
    state_[1] = 0.0f;
    state_[2] = 0.0f;
}

void SVFAnushriLinear::setInitialState(const Sample state[3]) {
    state_[0] = state[0];
    state_[1] = state[1];
    state_[2] = state[2];
}

void SVFAnushriLinear::setCutoffFreq(Sample freqHz) {
    const Sample sr = (sampleRate_ > 1.0f && std::isfinite(sampleRate_)) ? sampleRate_ : kSampleRate;
    const Sample nyquistSafe = sr * kNyquistCutoffRatio;
    const Sample freq = clamp(freqHz, 1.0f, nyquistSafe);
    if (freq == cutoffFreq_)
        return;
    cutoffFreq_ = freq;
    updateCoefficients();
}

void SVFAnushriLinear::setCutoffCV(Sample cv_dac, Sample cv_input, Sample cv_pot) {
    setCutoffFreq(ssm2164_.computeCutoffFreq(cv_dac, cv_input, cv_pot));
}

void SVFAnushriLinear::setResonanceCV(Sample cv) {
    Sample value;
    if (useAnuResonance_) {
        value = ssm2164_.anuResonanceGain(clamp(cv / 5.0f, 0.0f, 1.0f));
    } else {
        value = ssm2164_.resonanceGain(cv);
    }
    setResonance(value);
}

void SVFAnushriLinear::setResonance(Sample value) {
    const Sample res = clamp(value, 0.0f, 1.0f);
    if (res == resonance_)
        return;
    resonance_ = res;
    updateCoefficients();
}

void SVFAnushriLinear::setAuthenticMode(bool enabled) {
    authenticMode_ = enabled;
}

void SVFAnushriLinear::updateCoefficients() {
    const Sample sr = (sampleRate_ > 1.0f && std::isfinite(sampleRate_)) ? sampleRate_ : kSampleRate;
    const Sample fc = clamp(cutoffFreq_, 1.0f, sr * kNyquistCutoffRatio);
    g_ = std::tan(kPi * fc / sr);
    g_ = clamp(g_, 0.0001f, kMaxTptG);
    if (!std::isfinite(g_))
        g_ = 0.0001f;

    // TPT SVF damping: k = 1/Q. Resonance 0..1 maps Q = 1/(1-R).
    // R >= 1 => analog lossless case k = 0 (unit-circle poles). The nonlinear
    // wrapper saturates integrators so self-oscillation stays bounded.
    if (resonance_ >= 1.0f) {
        k_ = 0.0f;
        r_ = 2.0f;
        return;
    }

    const Sample effectiveQ = clamp(1.0f / (1.0f - resonance_), 0.5f, 100.0f);
    k_ = clamp(1.0f / effectiveQ, 0.0001f, 2.0f);
    r_ = 2.0f / k_;
}

Sample SVFAnushriLinear::processSelected(Sample input) {
    return process(input)[static_cast<uint8_t>(filterMode_)];
}

std::array<Sample, 3> SVFAnushriLinear::process(Sample input) {
    Sample ic2eq = state_[0];
    Sample ic1eq = state_[1];
    if (!std::isfinite(ic1eq) || !std::isfinite(ic2eq) || !std::isfinite(input)) {
        reset();
        return { 0.0f, 0.0f, 0.0f };
    }

    const Sample v0 = input - ic2eq;
    const Sample denom = 1.0f + g_ * (g_ + k_);
    const Sample invDen = (std::isfinite(denom) && denom > 1.0e-12f) ? (1.0f / denom) : 0.0f;
    const Sample v1 = (ic1eq + g_ * v0) * invDen;
    ic1eq = 2.0f * v1 - ic1eq;
    const Sample v2 = ic2eq + g_ * v1;
    ic2eq = 2.0f * v2 - ic2eq;

    const Sample lp = v2;
    const Sample bp = v1;
    const Sample hp = v0 - k_ * v1 - v2;

    if (!std::isfinite(ic1eq) || !std::isfinite(ic2eq) || !std::isfinite(lp)
        || !std::isfinite(bp) || !std::isfinite(hp)) {
        reset();
        return { 0.0f, 0.0f, 0.0f };
    }

    const Sample lpOut = clamp(lp, -kMaxIntegrator, kMaxIntegrator);
    const Sample bpOut = clamp(bp, -kMaxIntegrator, kMaxIntegrator);
    const Sample hpOut = clamp(hp, -kMaxIntegrator, kMaxIntegrator);

    state_[0] = clamp(ic2eq, -kMaxIntegrator, kMaxIntegrator);
    state_[1] = clamp(ic1eq, -kMaxIntegrator, kMaxIntegrator);
    state_[2] = hpOut;

    return { lpOut, bpOut, hpOut };
}

void SVFAnushriLinear::processBlock(const Sample* input, Sample* lpOut, Sample* bpOut, Sample* hpOut, size_t numFrames) {
    for (size_t i = 0; i < numFrames; ++i) {
        auto out = process(input[i]);
        lpOut[i] = out[0];
        bpOut[i] = out[1];
        hpOut[i] = out[2];
    }
}

SVFAnushriSingle::SVFAnushriSingle(Sample sampleRate, SVFOutput output)
    : svf_(sampleRate), outputType_(output) {}

void SVFAnushriSingle::setSampleRate(Sample sampleRate) {
    svf_.setSampleRate(sampleRate);
}

void SVFAnushriSingle::reset() {
    svf_.reset();
}

void SVFAnushriSingle::setCutoffFreq(Sample freqHz) {
    svf_.setCutoffFreq(freqHz);
}

void SVFAnushriSingle::setCutoffCV(Sample cv_dac, Sample cv_input, Sample cv_pot) {
    svf_.setCutoffCV(cv_dac, cv_input, cv_pot);
}

void SVFAnushriSingle::setResonance(Sample value) {
    svf_.setResonance(value);
}

void SVFAnushriSingle::setResonanceCV(Sample cv) {
    svf_.setResonanceCV(cv);
}

void SVFAnushriSingle::setOutputType(SVFOutput output) {
    outputType_ = output;
}

Sample SVFAnushriSingle::process(Sample input) {
    auto out = svf_.process(input);
    return out[static_cast<uint8_t>(outputType_)];
}

void SVFAnushriSingle::processBlock(const Sample* input, Sample* output, size_t numFrames) {
    for (size_t i = 0; i < numFrames; ++i) {
        output[i] = process(input[i]);
    }
}

} // namespace dsp
} // namespace lxx
