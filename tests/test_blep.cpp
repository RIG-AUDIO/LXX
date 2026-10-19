#include "../src/dsp/oscillators/blep_oscillator.h"
#include "../src/dsp/oscillators/sub_oscillator.h"
#include "../src/dsp/core/dsp_types.h"
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

void testBleP() {
    constexpr int kSampleRate = 48000;
    constexpr float kDuration = 0.1f; // Short 0.1 second
    constexpr size_t kNumFrames = kSampleRate * kDuration;
    
    printf("Testing BLEP Oscillator...\n");
    
    // Test 1: Triangle wave (authentic mode)
    {
        lxx::dsp::BlePOscillator osc(kSampleRate);
        osc.setWaveform(lxx::dsp::Waveform::TRIANGLE);
        osc.setFrequency(440.0f); // A4
        
        std::vector<float> output(kNumFrames);
        for (size_t i = 0; i < kNumFrames; ++i) {
            output[i] = osc.process();
        }
        
        writeWav("test_blep_triangle.wav", output.data(), kNumFrames);
        writeCsv("test_blep_triangle.csv", output.data(), kNumFrames);
        printf("  Triangle wave: done (%.3f sec)\n", kDuration);
    }
    
    // Test 2: Sawtooth wave (extended mode)
    {
        lxx::dsp::BlePOscillator osc(kSampleRate);
        osc.setWaveform(lxx::dsp::Waveform::SAWTOOTH);
        osc.setFrequency(440.0f);
        
        std::vector<float> output(kNumFrames);
        for (size_t i = 0; i < kNumFrames; ++i) {
            output[i] = osc.process();
        }
        
        writeWav("test_blep_sawtooth.wav", output.data(), kNumFrames);
        writeCsv("test_blep_sawtooth.csv", output.data(), kNumFrames);
        printf("  Sawtooth wave: done\n");
    }
    
    // Test 3: Pulse wave (extended mode, 50% duty)
    {
        lxx::dsp::BlePOscillator osc(kSampleRate);
        osc.setWaveform(lxx::dsp::Waveform::PULSE);
        osc.setFrequency(440.0f);
        osc.setPulseWidth(0.5f); // Square wave
        
        std::vector<float> output(kNumFrames);
        for (size_t i = 0; i < kNumFrames; ++i) {
            output[i] = osc.process();
        }
        
        writeWav("test_blep_pulse.wav", output.data(), kNumFrames);
        writeCsv("test_blep_pulse.csv", output.data(), kNumFrames);
        printf("  Pulse wave (50%%): done\n");
    }
    
    // Test 4: Pulse wave (25% duty)
    {
        lxx::dsp::BlePOscillator osc(kSampleRate);
        osc.setWaveform(lxx::dsp::Waveform::PULSE);
        osc.setFrequency(440.0f);
        osc.setPulseWidth(0.25f);
        
        std::vector<float> output(kNumFrames);
        for (size_t i = 0; i < kNumFrames; ++i) {
            output[i] = osc.process();
        }
        
        writeWav("test_blep_pulse_25.wav", output.data(), kNumFrames);
        writeCsv("test_blep_pulse_25.csv", output.data(), kNumFrames);
        printf("  Pulse wave (25%%): done\n");
    }
    
    // Test 5: High frequency test
    {
        lxx::dsp::BlePOscillator osc(kSampleRate);
        osc.setWaveform(lxx::dsp::Waveform::TRIANGLE);
        osc.setFrequency(12000.0f); // Near Nyquist/2
        
        std::vector<float> output(kNumFrames);
        for (size_t i = 0; i < kNumFrames; ++i) {
            output[i] = osc.process();
        }
        
        writeWav("test_blep_highfreq.wav", output.data(), kNumFrames);
        writeCsv("test_blep_highfreq.csv", output.data(), kNumFrames);
        printf("  High freq (12kHz): done\n");
    }
    
    // Test 6: Frequency sweep (short)
    {
        lxx::dsp::BlePOscillator osc(kSampleRate);
        osc.setWaveform(lxx::dsp::Waveform::TRIANGLE);
        
        std::vector<float> output(kNumFrames);
        for (size_t i = 0; i < kNumFrames; ++i) {
            float t = i / (float)kSampleRate;
            float freq = 55.0f * powf(2.0f, t * 6.0f); // 55Hz to ~3520Hz over 0.1s
            osc.setFrequency(freq);
            output[i] = osc.process();
        }
        
        writeWav("test_blep_sweep.wav", output.data(), kNumFrames);
        writeCsv("test_blep_sweep.csv", output.data(), kNumFrames);
        printf("  Frequency sweep: done\n");
    }
}

void testSubOsc() {
    constexpr int kSampleRate = 48000;
    constexpr float kDuration = 0.1f;
    constexpr size_t kNumFrames = kSampleRate * kDuration;
    
    printf("\nTesting Sub-Oscillator...\n");
    
    // Test 1: Basic sub-osc at 1/2 frequency
    {
        lxx::dsp::SubOscillator sub(kSampleRate);
        sub.setFrequency(220.0f); // A3 (main VCO at 440Hz)
        
        std::vector<float> output(kNumFrames);
        for (size_t i = 0; i < kNumFrames; ++i) {
            output[i] = sub.process();
        }
        
        writeWav("test_sub_osc.wav", output.data(), kNumFrames);
        writeCsv("test_sub_osc.csv", output.data(), kNumFrames);
        printf("  Sub-osc (220Hz): done\n");
    }
    
    // Test 2: Hard sync behavior
    {
        lxx::dsp::SubOscillator sub(kSampleRate);
        sub.setFrequency(220.0f);
        
        std::vector<float> output(kNumFrames);
        for (size_t i = 0; i < kNumFrames; ++i) {
            // Sync every 480 samples (100Hz = 10ms period)
            if (i % 480 == 0) {
                sub.hardSync();
            }
            output[i] = sub.process();
        }
        
        writeWav("test_sub_osc_sync.wav", output.data(), kNumFrames);
        writeCsv("test_sub_osc_sync.csv", output.data(), kNumFrames);
        printf("  Sub-osc with hard sync: done\n");
    }
    
    // Test 3: Phase-locked to main VCO simulation
    {
        lxx::dsp::BlePOscillator mainOsc(kSampleRate);
        lxx::dsp::SubOscillator subOsc(kSampleRate);
        
        mainOsc.setWaveform(lxx::dsp::Waveform::TRIANGLE);
        mainOsc.setFrequency(440.0f);
        subOsc.setFrequency(220.0f);
        
        std::vector<float> mainOut(kNumFrames);
        std::vector<float> subOut(kNumFrames);
        std::vector<float> mixed(kNumFrames);
        
        for (size_t i = 0; i < kNumFrames; ++i) {
            mainOut[i] = mainOsc.process();
            subOut[i] = subOsc.process();
            mixed[i] = mainOut[i] + 0.5f * subOut[i]; // Mix sub at -6dB
        }
        
        writeWav("test_main_plus_sub.wav", mixed.data(), kNumFrames);
        writeCsv("test_main_plus_sub.csv", mixed.data(), kNumFrames);
        printf("  Main + Sub mixed: done\n");
    }
}

int main() {
    testBleP();
    testSubOsc();
    printf("\nAll BLEP/Sub tests complete. Output files in build directory.\n");
    return 0;
}