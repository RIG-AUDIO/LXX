#include "../src/dsp/drums/drum_synth.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

void writeWav(const char* filename, const float* data, size_t numFrames, int sampleRate) {
    FILE* f = std::fopen(filename, "wb");
    if (!f) {
        return;
    }

    const uint32_t dataSize = static_cast<uint32_t>(numFrames * sizeof(float));
    const uint32_t fileSize = 36 + dataSize;

    std::fwrite("RIFF", 1, 4, f);
    std::fwrite(&fileSize, 4, 1, f);
    std::fwrite("WAVE", 1, 4, f);

    std::fwrite("fmt ", 1, 4, f);
    const uint32_t fmtSize = 16;
    std::fwrite(&fmtSize, 4, 1, f);
    const uint16_t audioFormat = 3;
    std::fwrite(&audioFormat, 2, 1, f);
    const uint16_t numChannels = 1;
    std::fwrite(&numChannels, 2, 1, f);
    std::fwrite(&sampleRate, 4, 1, f);
    const uint32_t byteRate = sampleRate * sizeof(float);
    std::fwrite(&byteRate, 4, 1, f);
    const uint16_t blockAlign = sizeof(float);
    std::fwrite(&blockAlign, 2, 1, f);
    const uint16_t bitsPerSample = 32;
    std::fwrite(&bitsPerSample, 2, 1, f);

    std::fwrite("data", 1, 4, f);
    std::fwrite(&dataSize, 4, 1, f);
    std::fwrite(data, sizeof(float), numFrames, f);
    std::fclose(f);
}

void writeCsv(const char* filename, const float* data, size_t numFrames) {
    FILE* f = std::fopen(filename, "w");
    if (!f) {
        return;
    }
    std::fprintf(f, "frame,value\n");
    for (size_t i = 0; i < numFrames; ++i) {
        std::fprintf(f, "%zu,%.10f\n", i, data[i]);
    }
    std::fclose(f);
}

struct HitMetrics {
    float peak = 0.0f;
    float rms = 0.0f;
    int attackSample = -1;
    float spectralCentroidHz = 0.0f;
};

HitMetrics analyzeHit(const float* data, size_t numFrames, int sampleRate) {
    HitMetrics m;
    double sumSq = 0.0;
    for (size_t i = 0; i < numFrames; ++i) {
        const float v = data[i];
        m.peak = std::max(m.peak, std::abs(v));
        sumSq += v * v;
        if (m.attackSample < 0 && std::abs(v) > 0.02f) {
            m.attackSample = static_cast<int>(i);
        }
    }
    m.rms = static_cast<float>(std::sqrt(sumSq / numFrames));

    constexpr int fftSize = 2048;
    if (numFrames < static_cast<size_t>(fftSize)) {
        return m;
    }

    double weighted = 0.0;
    double magnitudeSum = 0.0;
    for (int k = 1; k < fftSize / 2; ++k) {
        double re = 0.0;
        double im = 0.0;
        for (int n = 0; n < fftSize; ++n) {
            const double angle = -2.0 * M_PI * k * n / fftSize;
            re += data[n] * std::cos(angle);
            im += data[n] * std::sin(angle);
        }
        const double mag = std::sqrt(re * re + im * im);
        const double freq = static_cast<double>(k) * sampleRate / fftSize;
        weighted += freq * mag;
        magnitudeSum += mag;
    }
    if (magnitudeSum > 0.0) {
        m.spectralCentroidHz = static_cast<float>(weighted / magnitudeSum);
    }
    return m;
}

void renderInstrument(lxx::dsp::DrumSynth& synth,
                      uint8_t instrument,
                      const char* tag,
                      int sampleRate,
                      float durationSec) {
    synth.init(static_cast<float>(sampleRate));
    synth.seedRandom(0x21);
    synth.setBalance(128);
    synth.setBandwidth(255);
    synth.morphPatch(instrument, 128);
    synth.trigger(instrument, 255);

    const size_t numFrames = static_cast<size_t>(durationSec * sampleRate);
    std::vector<float> buffer(numFrames, 0.0f);
    synth.processBlock(buffer.data(), numFrames);

    char wavName[128];
    char csvName[128];
    std::snprintf(wavName, sizeof(wavName), "test_drums_%s.wav", tag);
    std::snprintf(csvName, sizeof(csvName), "test_drums_%s.csv", tag);
    writeWav(wavName, buffer.data(), numFrames, sampleRate);
    writeCsv(csvName, buffer.data(), numFrames);

    const auto metrics = analyzeHit(buffer.data(), numFrames, sampleRate);
    std::printf("  %s: peak=%.4f rms=%.4f attack=%d centroid=%.1f Hz\n",
                tag,
                metrics.peak,
                metrics.rms,
                metrics.attackSample,
                metrics.spectralCentroidHz);
}

}  // namespace

int main() {
    constexpr int kSampleRate = 48000;
    constexpr float kDuration = 1.5f;

    lxx::dsp::DrumSynth synth;
    std::printf("DrumSynth validation @ %d Hz\n", kSampleRate);

    renderInstrument(synth, 0, "bd", kSampleRate, kDuration);
    renderInstrument(synth, 1, "sd", kSampleRate, kDuration);
    renderInstrument(synth, 2, "hh", kSampleRate, kDuration);

    std::printf("Done. WAV/CSV written to cwd.\n");
    return 0;
}
