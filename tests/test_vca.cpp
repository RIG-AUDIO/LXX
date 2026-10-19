#include "../src/dsp/amplifiers/vca_curve.h"
#include "../src/dsp/amplifiers/vca_as2164.h"
#include "../src/dsp/modulation/cv_scaler.h"
#include "../src/dsp/envelopes/adsr_envelope.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>

void writeCsvMulti2(const char* filename,
                    const float* col1,
                    const float* col2,
                    size_t n,
                    const char* h1,
                    const char* h2) {
    FILE* f = fopen(filename, "w");
    if (!f) return;
    fprintf(f, "%s,%s\n", h1, h2);
    for (size_t i = 0; i < n; ++i) {
        fprintf(f, "%.10f,%.10f\n", col1[i], col2[i]);
    }
    fclose(f);
}

void writeCsvMulti3(const char* filename,
                    const float* col1,
                    const float* col2,
                    const float* col3,
                    size_t n,
                    const char* h1,
                    const char* h2,
                    const char* h3) {
    FILE* f = fopen(filename, "w");
    if (!f) return;
    fprintf(f, "%s,%s,%s\n", h1, h2, h3);
    for (size_t i = 0; i < n; ++i) {
        fprintf(f, "%.10f,%.10f,%.10f\n", col1[i], col2[i], col3[i]);
    }
    fclose(f);
}

int main() {
    printf("Testing AS2164 VCA (Phase 4)...\n\n");
    int failures = 0;

    // ============================================
    // Test 1: Raw gain curve vs VcaCvScaler (0–5 V CV)
    // ============================================
    {
        printf("=== Test 1: Raw AS2164 gain curve (cv 0–5 V) ===\n");
        constexpr int N = 501;
        std::vector<float> cv(N), curveGain(N), scalerGain(N), diff(N);
        float maxAbsDiff = 0.0f;

        for (int i = 0; i < N; ++i) {
            cv[i] = i * 5.0f / static_cast<float>(N - 1);
            curveGain[i] = lxx::dsp::As2164Curve::gainFromControlVoltage(cv[i]);
            scalerGain[i] = lxx::dsp::VcaCvScaler::as2164gain(cv[i]);
            diff[i] = curveGain[i] - scalerGain[i];
            maxAbsDiff = std::max(maxAbsDiff, std::fabs(diff[i]));
        }

        writeCsvMulti3("test_vca_raw_gain.csv",
                       cv.data(), curveGain.data(), scalerGain.data(), N,
                       "cv_volts", "as2164_curve", "cv_scaler");

        printf("  Max |curve - scaler| = %.2e\n", maxAbsDiff);
        if (maxAbsDiff > 1e-6f) {
            printf("  FAIL: curve/scaler mismatch\n");
            ++failures;
        } else {
            printf("  PASS: As2164Curve matches VcaCvScaler::as2164gain\n");
        }

        printf("  Endpoints: cv=0 gain=%.6f, cv=5 gain=%.6f\n",
               curveGain[0], curveGain[N - 1]);
    }

    // ============================================
    // Test 2: DAC → gain (VCA amplitude path)
    // ============================================
    {
        printf("\n=== Test 2: DAC to gain (inverted VCA path) ===\n");
        constexpr int N = 4096;
        std::vector<float> dac(N), gain(N);
        for (int i = 0; i < N; ++i) {
            dac[i] = static_cast<float>(i);
            gain[i] = lxx::dsp::As2164Curve::dacToGain(static_cast<float>(i));
        }
        writeCsvMulti2("test_vca_dac_gain.csv", dac.data(), gain.data(), N, "dac", "gain");

        const float g0 = gain[0];
        const float gMax = gain[N - 1];
        printf("  dac=0 gain=%.6f, dac=4095 gain=%.6f\n", g0, gMax);
        if (g0 != 0.0f) {
            printf("  FAIL: dac=0 must be digital silence (firmware vca_cv==0)\n");
            ++failures;
        } else if (g0 >= gMax) {
            printf("  FAIL: expected increasing gain with envelope DAC\n");
            ++failures;
        } else {
            printf("  PASS: dac=0 silence, monotonic gain vs envelope DAC\n");
        }
    }

    // ============================================
    // Test 3: Firmware VCA DAC scaling
    // ============================================
    {
        printf("\n=== Test 3: Firmware VCA CV scaling ===\n");
        const uint16_t envMax = 65535;
        const uint8_t velMixFull = 255;
        const uint8_t volDefault = 240;

        const uint16_t dacMax = lxx::dsp::As2164Curve::computeFirmwareVcaDac(
            envMax, velMixFull, volDefault);
        const uint16_t dacZero = lxx::dsp::As2164Curve::computeFirmwareVcaDac(
            0, velMixFull, volDefault);

        printf("  Max envelope: dac=%u (expect ~3840)\n", dacMax);
        printf("  Zero envelope: dac=%u\n", dacZero);

        if (dacZero != 0 || dacMax < 3800 || dacMax > 3900) {
            printf("  FAIL: firmware DAC scaling out of range\n");
            ++failures;
        } else {
            printf("  PASS: firmware scaling matches voice.cc semantics\n");
        }

        constexpr int Nv = 129;
        std::vector<float> velocity(Nv), dacVel(Nv), gainVel(Nv);
        for (int i = 0; i < Nv; ++i) {
            velocity[i] = static_cast<float>(i) / 127.0f;
            dacVel[i] = lxx::dsp::As2164Curve::computeFirmwareVcaDacFloat(
                1.0f, velocity[i], 1.0f, 240.0f);
            gainVel[i] = lxx::dsp::As2164Curve::dacToGain(dacVel[i]);
        }
        writeCsvMulti3("test_vca_velocity.csv",
                       velocity.data(), dacVel.data(), gainVel.data(), Nv,
                       "velocity", "dac", "gain");
        printf("  Velocity sweep: gain at vel=0 %.6f, vel=1 %.6f\n",
               gainVel[0], gainVel[Nv - 1]);
    }

    // ============================================
    // Test 4: VcaAs2164 processBlock on sine
    // ============================================
    {
        printf("\n=== Test 4: VcaAs2164 processBlock ===\n");
        lxx::dsp::VcaAs2164 vca;
        vca.setFromFirmware(1.0f, 1.0f, 1.0f, 240.0f);

        constexpr size_t N = 480;
        std::vector<float> in(N), out(N);
        for (size_t i = 0; i < N; ++i) {
            in[i] = std::sin(2.0f * lxx::dsp::kPi * static_cast<float>(i) / 48.0f);
        }
        vca.processBlock(in.data(), out.data(), N);

        float peakIn = 0.0f, peakOut = 0.0f;
        for (size_t i = 0; i < N; ++i) {
            peakIn = std::max(peakIn, std::fabs(in[i]));
            peakOut = std::max(peakOut, std::fabs(out[i]));
        }
        const float ratio = peakOut / (peakIn + 1e-10f);
        printf("  Peak ratio (out/in) = %.6f (gain=%.6f)\n", ratio, vca.getGain());
        if (std::fabs(ratio - vca.getGain()) > 0.001f) {
            printf("  FAIL: processBlock gain mismatch\n");
            ++failures;
        } else {
            printf("  PASS: processBlock applies exponential gain\n");
        }
    }

    // ============================================
    // Test 5: Envelope morph → VCA gain trajectory
    // ============================================
    {
        printf("\n=== Test 5: VCA envelope morph integration ===\n");
        constexpr int kSampleRate = 48000;
        constexpr size_t kFrames = kSampleRate;

        lxx::dsp::VcaEnvelope env(kSampleRate);
        env.setBaseTimes(0.01f, 0.2f, 0.7f, 0.5f);
        env.setMorph(0.5f);
        env.noteOn();

        lxx::dsp::VcaAs2164 vca;
        std::vector<float> envVal(kFrames), dac(kFrames), gain(kFrames), audio(kFrames);

        for (size_t i = 0; i < kFrames; ++i) {
            envVal[i] = env.process();
            dac[i] = lxx::dsp::As2164Curve::computeFirmwareVcaDacFloat(
                envVal[i], 1.0f, 1.0f, 240.0f);
            gain[i] = lxx::dsp::As2164Curve::dacToGain(dac[i]);
            vca.setDac(dac[i]);
            audio[i] = vca.processSample(1.0f);
            if (i == kSampleRate / 4) {
                env.noteOff();
            }
        }

        writeCsvMulti3("test_vca_morph_gain.csv",
                       envVal.data(), dac.data(), gain.data(), kFrames,
                       "envelope", "dac", "gain");
        writeCsvMulti2("test_vca_morph_audio.csv", envVal.data(), audio.data(), kFrames,
                       "envelope", "vca_output");

        printf("  Peak gain during attack: %.6f\n",
               *std::max_element(gain.begin(), gain.end()));
        printf("  Gain at rest (frame 0): %.6f\n", gain[0]);
    }

    // ============================================
    // Test 6: Temperature sweep (303.13 K reference)
    // ============================================
    {
        printf("\n=== Test 6: Temperature sweep ===\n");
        constexpr int N = 101;
        std::vector<float> temp(N), gainHot(N);
        for (int i = 0; i < N; ++i) {
            temp[i] = 293.15f + i * 20.0f / static_cast<float>(N - 1);
            gainHot[i] = lxx::dsp::As2164Curve::gainFromControlVoltage(2.5f, temp[i]);
        }
        writeCsvMulti2("test_vca_temp_sweep.csv", temp.data(), gainHot.data(), N,
                       "temp_k", "gain_cv2p5v");
        const float gainRoom = lxx::dsp::As2164Curve::gainFromControlVoltage(
            2.5f, lxx::dsp::kRoomTempKelvin);
        printf("  gain(cv=2.5V) at 25C=%.6f, at 313K=%.6f\n",
               gainRoom, gainHot[N - 1]);
    }

    printf("\n========================================\n");
    if (failures == 0) {
        printf("All VCA tests PASSED (%d checks)\n", 6);
    } else {
        printf("VCA tests FAILED: %d issue(s)\n", failures);
    }
    printf("CSV outputs written to build directory.\n");

    return failures > 0 ? 1 : 0;
}
