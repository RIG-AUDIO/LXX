#include "../src/dsp/core/oversampler.h"
#include <cstdio>
#include <vector>
#include <cmath>

// Use kPi from dsp_types
using lxx::dsp::kPi;

void testOversampler() {
    printf("Testing Oversampler...\n");
    
    // Test 2x
    {
        lxx::dsp::Oversampler os(lxx::dsp::Oversampler::Factor::x2);
        os.reset();
        
        // Test impulse response
        const int N = 1000;
        std::vector<float> in(N, 0.0f), out(N, 0.0f);
        in[0] = 1.0f; // Impulse
        
        os.processBlock(in.data(), out.data(), N);
        
        // Check output is reasonable (not all zeros, not NaN)
        float sum = 0.0f;
        for (float v : out) sum += std::abs(v);
        printf("  2x impulse response sum: %.6f\n", sum);
        
        // Test sine wave
        for (int i = 0; i < N; ++i) {
            in[i] = std::sin(2 * kPi * 1000 * i / 48000.0f); // 1 kHz at 48kHz
        }
        os.reset();
        os.processBlock(in.data(), out.data(), N);
        
        float rms = 0.0f;
        for (float v : out) rms += v * v;
        rms = std::sqrt(rms / N);
        printf("  2x 1kHz sine RMS: %.6f (expected ~0.707)\n", rms);
    }
    
    // Test 4x
    {
        lxx::dsp::Oversampler os(lxx::dsp::Oversampler::Factor::x4);
        os.reset();
        
        const int N = 1000;
        std::vector<float> in(N, 0.0f), out(N, 0.0f);
        in[0] = 1.0f;
        
        os.processBlock(in.data(), out.data(), N);
        
        float sum = 0.0f;
        for (float v : out) sum += std::abs(v);
        printf("  4x impulse response sum: %.6f\n", sum);
        
        for (int i = 0; i < N; ++i) {
            in[i] = std::sin(2 * kPi * 1000 * i / 48000.0f);
        }
        os.reset();
        os.processBlock(in.data(), out.data(), N);
        
        float rms = 0.0f;
        for (float v : out) rms += v * v;
        rms = std::sqrt(rms / N);
        printf("  4x 1kHz sine RMS: %.6f\n", rms);
    }
    
    // Test prepareBuffers/upsample/downsample
    {
        lxx::dsp::Oversampler os(lxx::dsp::Oversampler::Factor::x2);
        os.reset();
        
        const int N = 256;
        std::vector<float> in(N);
        for (int i = 0; i < N; ++i) {
            in[i] = std::sin(2 * kPi * 1000 * i / 48000.0f);
        }
        
        auto bufs = os.prepareBuffers(N);
        os.upsample(in.data(), N, bufs.up.data());
        
        // Process at 2x rate (just pass through for test)
        // In real use, this is where VCF runs at 96kHz
        
        os.downsample(bufs.up.data(), N, bufs.down.data());
        
        float rms = 0.0f;
        for (int i = 0; i < N; ++i) {
            rms += bufs.down[i] * bufs.down[i];
        }
        rms = std::sqrt(rms / N);
        printf("  Round-trip 2x RMS: %.6f\n", rms);
    }
    
    printf("Oversampler tests done.\n");
}

int main() {
    testOversampler();
    return 0;
}