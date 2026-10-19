#include "../src/dsp/lfo/lfo.h"
#include <cstdio>
#include <vector>
#include <cmath>
#include <fstream>
#include <algorithm>

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

// Statistical validation
void validateShape(const char* name, const float* data, size_t numFrames) {
    float minVal = 1.0f, maxVal = -1.0f, sum = 0.0f, sumSq = 0.0f;
    int zeroCrossings = 0;
    float prev = 0.0f;
    
    for (size_t i = 0; i < numFrames; ++i) {
        float v = data[i];
        minVal = std::min(minVal, v);
        maxVal = std::max(maxVal, v);
        sum += v;
        sumSq += v * v;
        if (i > 0 && prev * v < 0) zeroCrossings++;
        prev = v;
    }
    
    float mean = sum / numFrames;
    float rms = std::sqrt(sumSq / numFrames);
    
    printf("  %s: min=%.4f max=%.4f mean=%.4f rms=%.4f zero_cross=%d\n",
           name, minVal, maxVal, mean, rms, zeroCrossings);
}

int main() {
    constexpr int kSampleRate = 48000;
    constexpr float kDuration = 2.0f;
    constexpr size_t kNumFrames = kSampleRate * kDuration;
    
    printf("Testing LFO shapes...\n");
    
    lxx::dsp::Lfo lfo(kSampleRate);
    
    // Test each shape at 1 Hz
    const char* shapeNames[] = {
        "TRIANGLE", "SQUARE", "RAMP_UP", "RAMP_DOWN",
        "SAMPLE_HOLD", "BERNOULLI", "LINES", "NOISE"
    };
    
    for (int s = 0; s < 8; ++s) {
        lfo.setShape(s);
        lfo.setFrequency(1.0f); // 1 Hz
        lfo.reset();
        
        std::vector<float> output(kNumFrames);
        for (size_t i = 0; i < kNumFrames; ++i) {
            output[i] = lfo.processUnipolar(); // 0-1
        }
        
        char wavName[64], csvName[64];
        sprintf(wavName, "test_lfo_%s.wav", shapeNames[s]);
        sprintf(csvName, "test_lfo_%s.csv", shapeNames[s]);
        writeWav(wavName, output.data(), kNumFrames);
        writeCsv(csvName, output.data(), kNumFrames);
        
        validateShape(shapeNames[s], output.data(), kNumFrames);
    }
    
    // Test frequency sweep
    printf("\nTesting frequency sweep...\n");
    lfo.setShape(lxx::dsp::LfoShape::TRIANGLE);
    lfo.reset();
    
    std::vector<float> sweepOut(kNumFrames);
    for (size_t i = 0; i < kNumFrames; ++i) {
        float t = i / (float)kSampleRate;
        float freq = 0.1f + t * 5.0f; // 0.1 to 5 Hz over 2 seconds
        lfo.setFrequency(freq);
        sweepOut[i] = lfo.processUnipolar();
    }
    writeWav("test_lfo_sweep.wav", sweepOut.data(), kNumFrames);
    writeCsv("test_lfo_sweep.csv", sweepOut.data(), kNumFrames);
    validateShape("SWEEP", sweepOut.data(), kNumFrames);
    
    // Test MIDI clock sync (PLL)
    printf("\nTesting MIDI clock sync...\n");
    lfo.setShape(lxx::dsp::LfoShape::TRIANGLE);
    lfo.setFrequency(2.0f); // 2 Hz = 120 BPM quarter notes
    lfo.reset();
    
    std::vector<float> syncOut(kNumFrames);
    // Simulate MIDI clock at 120 BPM = 2 Hz = 48 clocks/sec
    // 24 clocks per quarter = 48 clocks/sec at 120 BPM
    // Target phase advances by 65536/24 = 2731 per clock
    for (size_t i = 0; i < kNumFrames; ++i) {
        // Every 1000 samples (48kHz/48 = 1000), simulate clock tick
        if (i % 1000 == 0) {
            static uint16_t clockPhase = 0;
            lfo.setTargetPhase(clockPhase);
            clockPhase = (clockPhase + 2731) & 0xFFFF;
        }
        syncOut[i] = lfo.processUnipolar();
    }
    writeWav("test_lfo_sync.wav", syncOut.data(), kNumFrames);
    writeCsv("test_lfo_sync.csv", syncOut.data(), kNumFrames);
    validateShape("SYNC", syncOut.data(), kNumFrames);
    
    // Test bipolar output
    printf("\nTesting bipolar output...\n");
    lfo.setShape(lxx::dsp::LfoShape::TRIANGLE);
    lfo.setFrequency(1.0f);
    lfo.reset();
    
    std::vector<float> bipolarOut(kSampleRate); // 1 second
    for (size_t i = 0; i < kSampleRate; ++i) {
        int16_t bi = lfo.processBipolar();
        bipolarOut[i] = bi / 32768.0f;
    }
    writeWav("test_lfo_bipolar.wav", bipolarOut.data(), kSampleRate);
    writeCsv("test_lfo_bipolar.csv", bipolarOut.data(), kSampleRate);
    validateShape("BIPOLAR", bipolarOut.data(), kSampleRate);
    
    // Symmetry check for triangle/ramp
    printf("\nSymmetry checks:\n");
    lfo.setShape(lxx::dsp::LfoShape::TRIANGLE);
    lfo.setFrequency(10.0f);
    lfo.reset();
    
    std::vector<float> triCheck(10000);
    for (size_t i = 0; i < 10000; ++i) {
        triCheck[i] = lfo.processUnipolar();
    }
    
    // Check triangle symmetry (should be symmetric around 0.5)
    float triMean = 0.0f;
    for (float v : triCheck) triMean += v;
    triMean /= triCheck.size();
    printf("  Triangle mean: %.6f (expected ~0.5)\n", triMean);
    
    // Check square wave duty cycle
    lfo.setShape(lxx::dsp::LfoShape::SQUARE);
    lfo.setFrequency(10.0f);
    lfo.reset();
    
    int highCount = 0;
    for (size_t i = 0; i < 10000; ++i) {
        if (lfo.processUnipolar() > 0.5f) highCount++;
    }
    printf("  Square duty cycle: %.4f (expected ~0.5)\n", highCount / 10000.0f);
    
    // S&H distribution check
    lfo.setShape(lxx::dsp::LfoShape::SAMPLE_HOLD);
    lfo.setFrequency(100.0f); // Fast
    lfo.reset();
    
    std::vector<uint16_t> shValues;
    shValues.reserve(10000);
    for (size_t i = 0; i < 10000; ++i) {
        shValues.push_back(lfo.process());
    }
    
    // Check uniform distribution (rough)
    std::sort(shValues.begin(), shValues.end());
    float shMean = 0.0f;
    for (uint16_t v : shValues) shMean += v;
    shMean /= shValues.size();
    printf("  S&H mean: %.2f (expected ~32767)\n", shMean);
    
    printf("\nAll LFO tests complete.\n");
    return 0;
}