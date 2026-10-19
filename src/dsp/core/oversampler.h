#pragma once

#include "dsp_types.h"
#include <array>
#include <vector>

namespace lxx {
namespace dsp {

/**
 * 2x/4x polyphase half-band FIR oversampler.
 * 127-tap Remez half-band; coeffs scaled sqrt(2)/sum(h) for unity round-trip.
 */
class Oversampler {
public:
    enum class Factor { x2, x4 };
    enum class Quality { Standard, High }; // High = 4x

    Oversampler() = default;
    explicit Oversampler(Factor factor, Quality quality = Quality::Standard);

    void reset();
    void setFactor(Factor factor);
    void setQuality(Quality quality);
    void prepare(size_t maxBaseFrames);

    Sample processSample(Sample input);
    void processBlock(const Sample* input, Sample* output, size_t numFrames);

    struct OversampledBuffers {
        std::vector<Sample> up;
        std::vector<Sample> down;
    };

    OversampledBuffers prepareBuffers(size_t baseFrames);
    void upsample(const Sample* input, size_t numFrames, Sample* upBuffer);
    void downsample(const Sample* upBuffer, size_t numFrames, Sample* output);

private:
    static constexpr int kFirLen = 127;
    static constexpr int kEvenLen = (kFirLen + 1) / 2; // 64 polyphase input taps
    static constexpr int kOddLen = kFirLen / 2;        // 63

    static constexpr std::array<Sample, kFirLen> kHalfBandFir = {
        -3.04634362e-03f,  1.28051019e-05f,  1.24770625e-03f,  1.01898795e-05f,
        -1.47183626e-03f,  1.18424518e-05f,  1.77144711e-03f,  8.99513272e-06f,
        -2.05914494e-03f,  1.44918268e-05f,  2.42765188e-03f,  6.31600302e-06f,
        -2.79044907e-03f,  1.45572298e-05f,  3.23998544e-03f,  8.21017349e-06f,
        -3.69101019e-03f,  1.38041648e-05f,  4.23648706e-03f,  5.44686221e-06f,
        -4.79367173e-03f,  1.61789814e-05f,  5.45660944e-03f,  7.48420712e-06f,
        -6.14461214e-03f,  1.82078568e-05f,  6.95215141e-03f,  4.44678679e-06f,
        -7.80483808e-03f,  1.67652298e-05f,  8.79938311e-03f,  3.57691586e-06f,
        -9.86887160e-03f,  1.64025207e-05f,  1.11168319e-02f,  4.27048398e-06f,
        -1.24854110e-02f,  1.56702035e-05f,  1.40947392e-02f,  4.85839935e-06f,
        -1.59080866e-02f,  1.59270614e-05f,  1.80755037e-02f,  4.10324938e-06f,
        -2.06050395e-02f,  1.67922566e-05f,  2.37204846e-02f,  3.41278613e-06f,
        -2.75495873e-02f,  1.76682449e-05f,  3.25173535e-02f,  3.05681812e-06f,
        -3.91347377e-02f,  1.82806928e-05f,  4.86066228e-02f,  3.18896593e-06f,
        -6.32510579e-02f,  1.77802897e-05f,  8.94050536e-02f,  3.05121185e-06f,
        -1.49904671e-01f,  1.74462665e-05f,  4.51150147e-01f,  7.08925527e-01f,
         4.51150147e-01f,  1.74462665e-05f, -1.49904671e-01f,  3.05121185e-06f,
         8.94050536e-02f,  1.77802897e-05f, -6.32510579e-02f,  3.18896593e-06f,
         4.86066228e-02f,  1.82806928e-05f, -3.91347377e-02f,  3.05681812e-06f,
         3.25173535e-02f,  1.76682449e-05f, -2.75495873e-02f,  3.41278613e-06f,
         2.37204846e-02f,  1.67922566e-05f, -2.06050395e-02f,  4.10324938e-06f,
         1.80755037e-02f,  1.59270614e-05f, -1.59080866e-02f,  4.85839935e-06f,
         1.40947392e-02f,  1.56702035e-05f, -1.24854110e-02f,  4.27048398e-06f,
         1.11168319e-02f,  1.64025207e-05f, -9.86887160e-03f,  3.57691586e-06f,
         8.79938311e-03f,  1.67652298e-05f, -7.80483808e-03f,  4.44678679e-06f,
         6.95215141e-03f,  1.82078568e-05f, -6.14461214e-03f,  7.48420712e-06f,
         5.45660944e-03f,  1.61789814e-05f, -4.79367173e-03f,  5.44686221e-06f,
         4.23648706e-03f,  1.38041648e-05f, -3.69101019e-03f,  8.21017349e-06f,
         3.23998544e-03f,  1.45572298e-05f, -2.79044907e-03f,  6.31600302e-06f,
         2.42765188e-03f,  1.44918268e-05f, -2.05914494e-03f,  8.99513272e-06f,
         1.77144711e-03f,  1.18424518e-05f, -1.47183626e-03f,  1.01898795e-05f,
         1.24770625e-03f,  1.28051019e-05f, -3.04634362e-03f
    };

    struct StageState {
        std::array<Sample, kEvenLen> upDelay{};
        std::array<Sample, kFirLen> downDelay{};
        int downCounter = 0;
    };

    Factor factor_ = Factor::x2;
    Quality quality_ = Quality::Standard;
    int oversampleRatio_ = 2;

    StageState stage1_;
    StageState stage2_;
    std::vector<Sample> x4Temp_;
    std::vector<Sample> processTemp_;

    void resetStage(StageState& stage);
    void upsampleStage(const Sample* input, size_t numFrames, Sample* upBuffer, StageState& stage);
    void downsampleStage(const Sample* upBuffer, size_t numHighSamples, Sample* output, StageState& stage);
};

} // namespace dsp
} // namespace lxx
