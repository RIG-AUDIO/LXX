#include "../src/dsp/envelopes/adsr_envelope.h"
#include "../src/dsp/envelopes/envelope_curves.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <fstream>

// Simple WAV writer (RIFF, 32-bit float)
void writeWav(const char* filename, const float* data, size_t numFrames, int sampleRate = 48000) {
    FILE* f = fopen(filename, "wb");
    if (!f) return;
    
    uint32_t dataSize = numFrames * sizeof(float);
    uint32_t fileSize = 36 + dataSize;
    
    // RIFF header
    fwrite("RIFF", 1, 4, f);
    fwrite(&fileSize, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    
    // fmt chunk
    fwrite("fmt ", 1, 4, f);
    uint32_t fmtSize = 16;
    fwrite(&fmtSize, 4, 1, f);
    uint16_t audioFormat = 3; // IEEE float
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
    
    // data chunk
    fwrite("data", 1, 4, f);
    fwrite(&dataSize, 4, 1, f);
    fwrite(data, sizeof(float), numFrames, f);
    
    fclose(f);
}

// Write CSV for comparison
void writeCsv(const char* filename, const float* data, size_t numFrames) {
    FILE* f = fopen(filename, "w");
    if (!f) return;
    
    fprintf(f, "frame,value\n");
    for (size_t i = 0; i < numFrames; ++i) {
        fprintf(f, "%zu,%.10f\n", i, data[i]);
    }
    fclose(f);
}

int main() {
    constexpr int kSampleRate = 48000;
    constexpr float kDuration = 3.0f; // 3 seconds
    constexpr size_t kNumFrames = kSampleRate * kDuration;
    
    printf("Testing ADSR Envelope...\n");
    
    // Test 1: Standard ADSR (matches Python ref: attack=0, decay=64, sustain=128, release=128)
    {
        lxx::dsp::AdsrEnvelope env(kSampleRate);
        env.setFromParams(0, 64, 128, 128);
        env.noteOn();
        
        std::vector<float> output(kNumFrames);
        for (size_t i = 0; i < kNumFrames; ++i) {
            output[i] = env.process();
            // Note off at 1 second (matches Python ref)
            if (i == kSampleRate) env.noteOff();
        }
        
        writeWav("test_adsr_standard.wav", output.data(), kNumFrames);
        writeCsv("test_adsr_standard.csv", output.data(), kNumFrames);
        printf("  Standard ADSR: done\n");
    }
    
    // Test 2: Fast AD (matches Python ref: attack=1, decay=20, sustain=0, release=20)
    {
        lxx::dsp::AdsrEnvelope env(kSampleRate);
        env.setFromParams(1, 20, 0, 20);
        env.noteOn();
        
        std::vector<float> output(kNumFrames);
        for (size_t i = 0; i < kNumFrames; ++i) {
            output[i] = env.process();
            if (i == kSampleRate) env.noteOff();
        }
        
        writeWav("test_adsr_fast_ad.wav", output.data(), kNumFrames);
        writeCsv("test_adsr_fast_ad.csv", output.data(), kNumFrames);
        printf("  Fast AD: done\n");
    }
    
    // Test 3: Long attack/release (matches Python ref: attack=200, decay=180, sustain=128, release=220)
    {
        lxx::dsp::AdsrEnvelope env(kSampleRate);
        env.setFromParams(200, 180, 128, 220);
        env.noteOn();
        
        std::vector<float> output(kNumFrames);
        for (size_t i = 0; i < kNumFrames; ++i) {
            output[i] = env.process();
            if (i == kSampleRate * 2) env.noteOff();
        }
        
        writeWav("test_adsr_long.wav", output.data(), kNumFrames);
        writeCsv("test_adsr_long.csv", output.data(), kNumFrames);
        printf("  Long times: done\n");
    }
    
    // Test 4: VCA Envelope with morph
    {
        lxx::dsp::VcaEnvelope vcaEnv(kSampleRate);
        vcaEnv.setBaseTimes(0.01f, 0.2f, 0.7f, 0.5f);
        
        // Test morph = 0 (AD)
        vcaEnv.setMorph(0.0f);
        vcaEnv.noteOn();
        std::vector<float> outAd(kNumFrames);
        for (size_t i = 0; i < kNumFrames; ++i) {
            outAd[i] = vcaEnv.process();
            if (i == kSampleRate) vcaEnv.noteOff();
        }
        writeWav("test_vca_morph0.wav", outAd.data(), kNumFrames);
        writeCsv("test_vca_morph0.csv", outAd.data(), kNumFrames);
        
        // Test morph = 0.5 (ADSR)
        vcaEnv.setMorph(0.5f);
        vcaEnv.noteOn();
        std::vector<float> outAdsr(kNumFrames);
        for (size_t i = 0; i < kNumFrames; ++i) {
            outAdsr[i] = vcaEnv.process();
            if (i == kSampleRate) vcaEnv.noteOff();
        }
        writeWav("test_vca_morph50.wav", outAdsr.data(), kNumFrames);
        writeCsv("test_vca_morph50.csv", outAdsr.data(), kNumFrames);
        
        // Test morph = 1.0 (fast AD)
        vcaEnv.setMorph(1.0f);
        vcaEnv.noteOn();
        std::vector<float> outFast(kNumFrames);
        for (size_t i = 0; i < kNumFrames; ++i) {
            outFast[i] = vcaEnv.process();
            if (i == kSampleRate) vcaEnv.noteOff();
        }
        writeWav("test_vca_morph100.wav", outFast.data(), kNumFrames);
        writeCsv("test_vca_morph100.csv", outFast.data(), kNumFrames);
        
        printf("  VCA morph: done\n");
    }
    
    // Test 5: Mod Envelope (AD only)
    {
        lxx::dsp::ModEnvelope modEnv(kSampleRate);
        modEnv.setTimes(0.001f, 0.1f);
        modEnv.noteOn();
        
        std::vector<float> output(kNumFrames);
        for (size_t i = 0; i < kNumFrames; ++i) {
            output[i] = modEnv.process();
            if (i == kSampleRate) modEnv.noteOff();
        }
        
        writeWav("test_mod_env.wav", output.data(), kNumFrames);
        writeCsv("test_mod_env.csv", output.data(), kNumFrames);
        printf("  Mod envelope: done\n");
    }
    
    // Test 6: Parameter-based setup (from 8-bit params)
    {
        lxx::dsp::AdsrEnvelope env(kSampleRate);
        // Simulate original params: attack=0, decay=64, sustain=128, release=128
        env.setFromParams(0, 64, 128, 128);
        env.noteOn();
        
        std::vector<float> output(kNumFrames);
        for (size_t i = 0; i < kNumFrames; ++i) {
            output[i] = env.process();
            if (i == kSampleRate) env.noteOff();
        }
        
        writeWav("test_adsr_params.wav", output.data(), kNumFrames);
        writeCsv("test_adsr_params.csv", output.data(), kNumFrames);
        printf("  Param-based: done\n");
    }
    
    // Test 7: Curve types
    {
        lxx::dsp::AdsrEnvelope env(kSampleRate);
        env.setTimes(0.1f, 0.2f, 0.5f, 0.3f);
        
        for (int c = 0; c < 4; ++c) {
            env.setCurveType(static_cast<lxx::dsp::EnvCurveType>(c));
            env.noteOn();
            
            std::vector<float> output(kSampleRate); // 1 second
            for (size_t i = 0; i < kSampleRate; ++i) {
                output[i] = env.process();
            }
            env.noteOff();
            for (size_t i = kSampleRate; i < kSampleRate * 2; ++i) {
                output.push_back(env.process());
            }
            
            char name[64];
            sprintf(name, "test_curve_%d.wav", c);
            writeWav(name, output.data(), output.size());
            sprintf(name, "test_curve_%d.csv", c);
            writeCsv(name, output.data(), output.size());
        }
        printf("  Curve types: done\n");
    }
    
    // Test 8: Retrigger (legato)
    {
        lxx::dsp::AdsrEnvelope env(kSampleRate);
        env.setTimes(0.05f, 0.1f, 0.7f, 0.2f);
        env.noteOn();
        
        std::vector<float> output(kNumFrames);
        for (size_t i = 0; i < kNumFrames; ++i) {
            output[i] = env.process();
            // Retrigger at 0.5s, 1.0s, 1.5s
            if (i == kSampleRate/2 || i == kSampleRate || i == kSampleRate*3/2) {
                env.retrigger();
            }
        }
        
        writeWav("test_retrigger.wav", output.data(), kNumFrames);
        writeCsv("test_retrigger.csv", output.data(), kNumFrames);
        printf("  Retrigger: done\n");
    }
    
    printf("\nAll envelope tests complete. Output files in build directory.\n");
    return 0;
}