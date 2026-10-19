#include "../src/dsp/filters/svf_anushri_nonlinear.h"
#include "../src/dsp/filters/svf_anushri.h"
#include <cstdio>
#include <vector>
#include <cmath>

// Simple WAV writer
void writeWav(const char* filename, const float* data, size_t numFrames, int sampleRate = 48000) {
    FILE* f = fopen(filename, "wb");
    if (!f) return;
    uint32_t dataSize = numFrames * sizeof(float);
    uint32_t fileSize = 36 + dataSize;
    fwrite("RIFF", 1, 4, f);
    fwrite(&fileSize, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    uint32_t fmtSize = 16;
    fwrite(&fmtSize, 4, 1, f);
    uint16_t audioFormat = 3;
    fwrite(&audioFormat, 2, 1, f);
    uint16_t numChannels = 1;
    fwrite(&numChannels, 2, 1, f);
    fwrite(&sampleRate, 4, 1, f);
    uint32_t byteRate = sampleRate * sizeof(float);
    fwrite(&byteRate, 4, 1, f);
    uint16_t blockAlign = sizeof(float);
    fwrite(&blockAlign, 2, 1, f);
    uint16_t bitsPerSample = 32;
    fwrite(&bitsPerSample, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&dataSize, 4, 1, f);
    fwrite(data, sizeof(float), numFrames, f);
    fclose(f);
}

// Write CSV
void writeCsv(const char* filename, const float* data, size_t numFrames) {
    FILE* f = fopen(filename, "w");
    if (!f) return;
    fprintf(f, "frame,value\n");
    for (size_t i = 0; i < numFrames; ++i) {
        fprintf(f, "%zu,%.10f\n", i, data[i]);
    }
    fclose(f);
}

// Analyze self-oscillation: find peaks and measure settled amplitude
void analyzeOscillation(const float* output, size_t numFrames, int sampleRate, const char* testName) {
    // Overall max
    float maxAmp = 0.0f;
    for (size_t i = 0; i < numFrames; ++i) maxAmp = std::max(maxAmp, std::abs(output[i]));
    
    // Check settled portion (last 1 second)
    size_t settledStart = numFrames - sampleRate;
    float settledMax = 0.0f, settledRms = 0.0f;
    for (size_t i = settledStart; i < numFrames; ++i) {
        float a = std::abs(output[i]);
        settledMax = std::max(settledMax, a);
        settledRms += output[i] * output[i];
    }
    settledRms = std::sqrt(settledRms / (numFrames - settledStart));
    
    // Find peaks in settled region
    int peakCount = 0;
    float peakAmpSum = 0.0f;
    size_t lastPeakIdx = 0;
    for (size_t i = numFrames - 48000 + 1; i < numFrames - 1; ++i) {
        if (output[i] > output[i-1] && output[i] > output[i+1] && output[i] > 0.001f) {
            peakCount++;
            peakAmpSum += output[i];
            lastPeakIdx = i;
        }
    }
    float avgPeakAmp = (peakCount > 0) ? peakAmpSum / peakCount : 0.0f;
    
    // Print results
    printf("  %s:\n", testName);
    printf("  Overall max: %.6f\n", maxAmp);
    printf("  Settled max (last 1s): %.6f\n", settledMax);
    printf("  Settled RMS (last 1s): %.6f\n", settledRms);
    printf("  Peak count (last 1s): %d\n", peakCount);
    if (peakCount > 0) {
        printf("  Avg peak amplitude: %.6f\n", avgPeakAmp);
        printf("  Last peak at sample %zu: %.6f\n", lastPeakIdx, output[lastPeakIdx]);
    }
    printf("  Self-oscillating: %s\n", (settledMax > 0.001f && peakCount > 10) ? "YES" : "NO");
}

int main() {
    constexpr int kSampleRate = 48000;
    constexpr float kDuration = 3.0f;
    constexpr size_t kNumFrames = kSampleRate * kDuration;
    
    printf("Testing Filter Self-Oscillation (TPT SVF, k=0 at resonance=1.0)...\n\n");
    
    // Test 1: Linear core impulse response at resonance=1.0 (k=0)
    {
        printf("Test 1: Linear Core - Impulse at resonance=1.0 (k=0)...\n");
        lxx::dsp::SVFAnushriLinear svf(48000.0f);
        svf.setCutoffFreq(1000.0f);
        svf.setResonance(1.0f); // Max resonance
        svf.reset();
        
        printf("  Debug: g=%.6f, k=%.6f, r=%.6f, res=%.6f\n",
               svf.getG(), svf.getK(), svf.getR(), svf.getResonance());
        
        std::vector<float> output(kNumFrames);
        for (size_t i = 0; i < kNumFrames; ++i) {
            float in = (i == 0) ? 1.0f : 0.0f;
            auto out = svf.process(in);
            output[i] = out[1];  // BP — limit cycle on bandpass path
        }
        
        // Print first 50 samples
        printf("  First 30 samples:\n");
        for (size_t i = 0; i < 30; ++i) {
            printf("  [%zu] %.10f\n", i, output[i]);
        }
        
        analyzeOscillation(output.data(), kNumFrames, 48000, "Linear Core (k=0)");
        
        // Print first 100 samples to CSV for external analysis
        FILE* f = fopen("test_linear_impulse.csv", "w");
        if (f) {
            fprintf(f, "sample,value\n");
            for (size_t i = 0; i < 200; ++i) fprintf(f, "%zu,%.10f\n", i, output[i]);
            fclose(f);
        }
        printf("  Done\n\n");
    }
    
    // Test 2: Non-linear core with saturation at resonance=0.95 (sine sweep)
    {
        printf("\nTest 2: Non-Linear Core - Sine sweep at resonance=0.95...\n");
        lxx::dsp::SVFAnushriNonlinear svf(48000.0f);
        svf.setCutoffFreq(1000.0f);
        svf.setResonance(0.95f);
        svf.reset();
        
        std::vector<float> output(kNumFrames);
        for (size_t i = 0; i < kNumFrames; ++i) {
            float t = i / 48000.0f;
            float in = 0.01f * std::sin(2 * 3.14159265359f * 1000.0f * t);
            auto out = svf.process(in);
            output[i] = out[1];  // BP — limit cycle on bandpass path
        }
        
        analyzeOscillation(output.data(), kNumFrames, 48000, "Non-Linear (res=0.95)");
        printf("  Done\n\n");
    }
    
    // Test 3: Non-linear core with saturation at resonance=1.0 (impulse)
    {
        printf("\nTest 3: Non-Linear Core - Impulse at resonance=1.0 (with saturation)...\n");
        lxx::dsp::SVFAnushriNonlinear svf(48000.0f);
        svf.setCutoffFreq(1000.0f);
        svf.setResonance(1.0f);
        svf.reset();
        
        std::vector<float> output(kNumFrames);
        for (size_t i = 0; i < kNumFrames; ++i) {
            float in = (i == 0) ? 1.0f : 0.0f;
            auto out = svf.process(in);
            output[i] = out[1];  // BP — limit cycle on bandpass path
        }
        
        analyzeOscillation(output.data(), kNumFrames, 48000, "Non-Linear (res=1.0, saturated)");
        printf("  Done\n\n");
    }
    
    // Test 4: Non-linear core at resonance=1.0 WITHOUT saturation (pure linear behavior)
    {
        printf("\nTest 4: Non-Linear Core - Impulse at resonance=1.0 (NO saturation)...\n");
        lxx::dsp::SVFAnushriNonlinear svf(48000.0f);
        svf.setCutoffFreq(1000.0f);
        svf.setResonance(1.0f);
        svf.reset();
        
        // Disable all saturation
        lxx::dsp::SVFNonlinearCalibration cal;
        cal.enableInputSaturation = false;
        cal.enableIntegratorSaturation = false;
        cal.enableResonanceNonlinearity = false;
        svf.setCalibration(cal);
        
        std::vector<float> output(kNumFrames);
        for (size_t i = 0; i < kNumFrames; ++i) {
            float in = (i == 0) ? 1.0f : 0.0f;
            auto out = svf.process(in);
            output[i] = out[1];  // BP — limit cycle on bandpass path
        }
        
        analyzeOscillation(output.data(), kNumFrames, 48000, "Non-Linear NO SAT (res=1.0)");
        printf("  Done\n\n");
    }
    
    // Test 5: Step response at resonance=1.0
    {
        printf("\nTest 5: Step response at resonance=1.0...\n");
        constexpr int kSampleRate = 48000;
        constexpr float kStepDur = 2.0f;
        constexpr size_t kStepFrames = kSampleRate * kStepDur;
        
        lxx::dsp::SVFAnushriNonlinear svf(48000.0f);
        svf.setCutoffFreq(500.0f);
        svf.setResonance(1.0f);
        svf.reset();
        
        std::vector<float> output(kStepFrames);
        for (size_t i = 0; i < kStepFrames; ++i) {
            float in = (i < 4800) ? 0.5f : 0.0f; // 100ms step
            auto out = svf.process(in);
            output[i] = out[1];  // BP — limit cycle on bandpass path
        }
        
        float maxAmp = 0.0f;
        for (float v : output) maxAmp = std::max(maxAmp, std::abs(v));
        printf("  Max amplitude: %.6f\n", maxAmp);
        
        float settledMax = 0.0f;
        for (size_t i = kStepFrames - 24000; i < kStepFrames; ++i) {
            settledMax = std::max(settledMax, std::abs(output[i]));
        }
        printf("  Settled max (last 0.5s): %.6f\n", settledMax);
        
        writeWav("test_step_response_nl.wav", output.data(), kStepFrames);
        printf("  Done\n\n");
    }
    
    // Test 6: Resonance transition zone (NL, impulse, 1 kHz)
    {
        printf("\nTest 6: Resonance transition (NL authentic, impulse, fc=1kHz)...\n");
        const float resLevels[] = {0.5f, 0.8f, 0.9f, 0.95f, 0.99f, 0.999f, 1.0f};
        printf("  res     | k        | settled BP peak\n");
        for (float res : resLevels) {
            lxx::dsp::SVFAnushriNonlinear svf(48000.0f);
            svf.setAuthenticMode(true);
            svf.setCutoffFreq(1000.0f);
            svf.setResonance(res);
            svf.reset();

            std::vector<float> output(kNumFrames);
            for (size_t i = 0; i < kNumFrames; ++i) {
                float in = (i == 0) ? 1.0f : 0.0f;
                auto out = svf.process(in);
                output[i] = out[1];
            }

            size_t settledStart = kNumFrames - kSampleRate;
            float settledMax = 0.0f;
            for (size_t i = settledStart; i < kNumFrames; ++i) {
                settledMax = std::max(settledMax, std::abs(output[i]));
            }
            const auto& core = svf.getLinearCore();
            printf("  %.5f | %8.6f | %.6f\n", res, core.getK(), settledMax);
        }
        printf("  Done\n\n");
    }

    printf("\nAll self-oscillation tests complete.\n");
    return 0;
}