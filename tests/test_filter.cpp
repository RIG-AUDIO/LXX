#include "../src/dsp/filters/ssm2164_model.h"
#include "../src/dsp/filters/svf_anushri.h"
#include "../src/dsp/filters/svf_anushri_nonlinear.h"
#include "../src/dsp/filters/oversampled_filter.h"
#include <cstdio>
#include <vector>
#include <cmath>
#include <fstream>

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

// Write CSV with multiple columns
void writeCsvMulti(const char* filename, const float* col1, const float* col2, size_t numFrames, const char* h1, const char* h2) {
    FILE* f = fopen(filename, "w");
    if (!f) return;
    fprintf(f, "%s,%s\n", h1, h2);
    for (size_t i = 0; i < numFrames; ++i) {
        fprintf(f, "%.10f,%.10f\n", col1[i], col2[i]);
    }
    fclose(f);
}

int main() {
    printf("Testing SSM2164 Model & SVF...\n");
    
    // ============================================
    // Step 1: SSM2164 Model Validation
    // ============================================
    {
        printf("\n=== Step 1: SSM2164 Model ===\n");
        lxx::dsp::SSM2164Model ssm;
        
        // Test 1a: Cutoff gain curve
        {
            printf("  Cutoff gain curve...\n");
            const int N = 1001;
            std::vector<float> cvs(N), gains(N);
            for (int i = 0; i < N; ++i) {
                cvs[i] = -2.0f + i * 4.0f / (N - 1); // -2V to +2V
                gains[i] = ssm.cutoffGain(cvs[i]);
            }
            writeCsvMulti("test_ssm2164_cutoff_gain.csv", cvs.data(), gains.data(), N, "cv", "gain");
            printf("    Done: %d points\n", N);
        }
        
        // Test 1b: Resonance gain curve (SSM2164)
        {
            printf("  Resonance gain curve (SSM2164)...\n");
            const int N = 1001;
            std::vector<float> cvs(N), gains(N);
            for (int i = 0; i < N; ++i) {
                cvs[i] = i * 5.0f / (N - 1); // 0-5V
                gains[i] = ssm.resonanceGain(cvs[i]);
            }
            writeCsvMulti("test_ssm2164_resonance_gain.csv", cvs.data(), gains.data(), N, "cv", "gain");
            printf("    Done: %d points\n", N);
        }
        
        // Test 1c: Anushri resonance curve
        {
            printf("  Anushri resonance curve...\n");
            const int N = 1001;
            std::vector<float> vals(N), gains(N);
            for (int i = 0; i < N; ++i) {
                vals[i] = i / 1000.0f;
                gains[i] = ssm.anuResonanceGain(vals[i]);
            }
            writeCsvMulti("test_ssm2164_anu_resonance.csv", vals.data(), gains.data(), N, "value", "gain");
            printf("    Done: %d points\n", N);
        }
        
        // Test 1d: Cutoff frequency from CV
        {
            printf("  Cutoff freq from CV...\n");
            const int N = 501;
            std::vector<float> cvs(N), freqs(N);
            for (int i = 0; i < N; ++i) {
                cvs[i] = i * 5.0f / (N - 1);
                freqs[i] = ssm.computeCutoffFreq(cvs[i], 0.0f, 2.5f);
            }
            writeCsvMulti("test_ssm2164_cutoff_freq.csv", cvs.data(), freqs.data(), N, "cv_dac", "freq");
            printf("    Max freq: %.1f Hz\n", ssm.getMaxFreq());
        }
    }
    
    // ============================================
    // Step 2: SVF Linear Core Validation
    // ============================================
    {
        printf("\n=== Step 2: SVF Linear Core ===\n");
        lxx::dsp::SVFAnushriLinear svf(48000.0f);
        
        // Test 2a: Frequency response sweep
        {
            printf("  Frequency response sweep...\n");
            const int N = 200;
            std::vector<float> freqs(N), lpMag(N), bpMag(N), hpMag(N);
            
            for (int i = 0; i < N; ++i) {
                // Log sweep from 20Hz to 20kHz
                freqs[i] = 20.0f * std::pow(1000.0f, i / 199.0f);
                svf.setCutoffFreq(freqs[i]);
                svf.setResonance(0.1f); // Low resonance
                
                // Impulse response to get frequency response
                // Simpler: just check steady-state gain at various input frequencies
                // For now, test step response
                svf.reset();
                float lp = 0, bp = 0, hp = 0;
                for (int n = 0; n < 1000; ++n) {
                    float in = (n == 0) ? 1.0f : 0.0f;
                    auto out = svf.process(in);
                    lp = out[0];
                    bp = out[1];
                    hp = out[2];
                }
                lpMag[i] = std::abs(lp);
                bpMag[i] = std::abs(bp);
                hpMag[i] = std::abs(hp);
            }
            writeCsvMulti("test_svf_linear_freq_resp.csv", freqs.data(), lpMag.data(), N, "freq", "lp_gain");
            writeCsvMulti("test_svf_linear_bp_resp.csv", freqs.data(), bpMag.data(), N, "freq", "bp_gain");
            writeCsvMulti("test_svf_linear_hp_resp.csv", freqs.data(), hpMag.data(), N, "freq", "hp_gain");
            printf("    Done: %d frequencies\n", N);
        }
        
        // Test 2b: Resonance sweep at fixed cutoff
        {
            printf("  Resonance sweep...\n");
            svf.setCutoffFreq(1000.0f);
            const int N = 100;
            std::vector<float> resVals(N), bpPeak(N);
            
            for (int i = 0; i < N; ++i) {
                float res = i / 99.0f;
                svf.setResonance(res);
                svf.reset();
                
                // Find peak of bandpass impulse response
                float maxBp = 0;
                for (int n = 0; n < 2000; ++n) {
                    float in = (n == 0) ? 1.0f : 0.0f;
                    auto out = svf.process(in);
                    maxBp = std::max(maxBp, std::abs(out[1]));
                }
                bpPeak[i] = maxBp;
                resVals[i] = res;
            }
            writeCsvMulti("test_svf_linear_resonance_sweep.csv", resVals.data(), bpPeak.data(), N, "resonance", "bp_peak");
            printf("    Done: %d resonance values\n", N);
        }
        
        // Test 2c: Self-oscillation stability test
        {
            printf("  Self-oscillation stability...\n");
            svf.setCutoffFreq(1000.0f);
            svf.setResonance(1.0f); // Max resonance
            svf.reset();
            
            std::vector<float> output(48000); // 1 second
            for (size_t i = 0; i < output.size(); ++i) {
                auto out = svf.process(0.0f); // No input
                output[i] = out[0]; // LP output
            }
            
            // Check amplitude doesn't blow up
            float maxAmp = 0;
            for (float v : output) maxAmp = std::max(maxAmp, std::abs(v));
            printf("    Max amplitude: %.6f\n", maxAmp);
            writeWav("test_svf_self_osc.wav", output.data(), output.size());
            writeCsv("test_svf_self_osc.csv", output.data(), output.size());
        }
    }
    
    // ============================================
    // Step 3: SVF Non-Linear Validation
    // ============================================
    {
        printf("\n=== Step 3: SVF Non-Linear ===\n");
        lxx::dsp::SVFAnushriNonlinear svf(48000.0f);
        
        // Test 3a: Resonant sweep with saturation
        {
            printf("  Resonant sweep with saturation...\n");
            svf.setCutoffFreq(1000.0f);
            
            // Low resonance
            svf.setResonance(0.1f);
            svf.reset();
            std::vector<float> outLow(48000);
            for (size_t i = 0; i < outLow.size(); ++i) {
                float in = 0.1f * std::sin(2 * 3.14159f * 1000.0f * i / 48000.0f);
                outLow[i] = svf.process(in)[0];
            }
            writeWav("test_svf_nl_low_res.wav", outLow.data(), outLow.size());
            printf("    Low resonance (0.1): done\n");
            
            // Medium resonance
            svf.setResonance(0.5f);
            svf.reset();
            std::vector<float> outMed(48000);
            for (size_t i = 0; i < outMed.size(); ++i) {
                float in = 0.1f * std::sin(2 * 3.14159f * 1000.0f * i / 48000.0f);
                outMed[i] = svf.process(in)[0];
            }
            writeWav("test_svf_nl_med_res.wav", outMed.data(), outMed.size());
            printf("    Medium resonance (0.5): done\n");
            
            // High resonance (near self-oscillation)
            svf.setResonance(0.9f);
            svf.reset();
            std::vector<float> outHigh(48000);
            for (size_t i = 0; i < outHigh.size(); ++i) {
                float in = 0.01f * std::sin(2 * 3.14159f * 1000.0f * i / 48000.0f);
                outHigh[i] = svf.process(in)[0];
            }
            writeWav("test_svf_nl_high_res.wav", outHigh.data(), outHigh.size());
            printf("    High resonance (0.9): done\n");
        }
        
        // Test 3b: Filter "ping" - impulse at high resonance
        {
            printf("  Filter ping (impulse at high resonance)...\n");
            svf.setCutoffFreq(500.0f);
            svf.setResonance(0.95f);
            svf.reset();
            
            std::vector<float> pingOut(48000);
            for (size_t i = 0; i < pingOut.size(); ++i) {
                float in = (i == 0) ? 1.0f : 0.0f;
                pingOut[i] = svf.process(in)[1]; // Bandpass
            }
            writeWav("test_svf_filter_ping.wav", pingOut.data(), pingOut.size());
            writeCsv("test_svf_filter_ping.csv", pingOut.data(), pingOut.size());
            printf("    Done\n");
        }
        
        // Test 3c: Cutoff sweep with resonance
        {
            printf("  Cutoff sweep with resonance...\n");
            svf.setResonance(0.7f);
            svf.reset();
            
            std::vector<float> sweepOut(48000);
            for (size_t i = 0; i < sweepOut.size(); ++i) {
                float t = i / 48000.0f;
                float freq = 100.0f * std::pow(100.0f, t); // 100Hz to 10kHz
                svf.setCutoffFreq(freq);
                float in = 0.05f * std::sin(2 * 3.14159f * freq * t);
                sweepOut[i] = svf.process(in)[0];
            }
            writeWav("test_svf_cutoff_sweep.wav", sweepOut.data(), sweepOut.size());
            printf("    Done\n");
        }
    }
    
    // ============================================
    // Step 4: Oversampled Filter (quick test)
    // ============================================
    {
        printf("\n=== Step 4: Oversampled Filter (2x) ===\n");
        lxx::dsp::OversampledSVF osf(48000.0f, lxx::dsp::OversampledSVF::Quality::Standard);
        
        osf.setCutoffFreq(1000.0f);
        osf.setResonance(0.5f);
        
        std::vector<float> out(48000);
        for (size_t i = 0; i < out.size(); ++i) {
            float in = 0.1f * std::sin(2 * 3.14159f * 1000.0f * i / 48000.0f);
            out[i] = osf.process(in)[0];
        }
        writeWav("test_osf_2x.wav", out.data(), out.size());
        printf("  2x oversampled filter: done\n");
    }
    
    printf("\nAll filter tests complete. Output files in build directory.\n");
    return 0;
}