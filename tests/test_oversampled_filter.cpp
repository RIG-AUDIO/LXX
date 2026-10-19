#include "../src/dsp/filters/oversampled_filter.h"
#include "../src/dsp/filters/svf_anushri_nonlinear.h"
#include "../src/dsp/filters/svf_anushri.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>

static void writeCsv(const char* filename, const float* data, size_t n) {
    FILE* f = fopen(filename, "w");
    if (!f) return;
    fprintf(f, "sample,value\n");
    for (size_t i = 0; i < n; ++i) {
        fprintf(f, "%zu,%.10f\n", i, data[i]);
    }
    fclose(f);
}

static float settledPeak(const float* data, size_t n, size_t sampleRate) {
    const size_t start = n > sampleRate ? n - sampleRate : 0;
    float peak = 0.0f;
    for (size_t i = start; i < n; ++i) {
        peak = std::max(peak, std::abs(data[i]));
    }
    return peak;
}

static void renderSineDrive(
    const char* tag,
    float fc,
    float resonance,
    size_t numFrames,
    int sampleRate,
    bool useOversampled,
    int osFactor) {
    std::vector<float> out(numFrames);

    lxx::dsp::SVFAnushriNonlinear nlFilter(static_cast<float>(sampleRate));
    nlFilter.setAuthenticMode(true);
    nlFilter.setCutoffFreq(fc);
    nlFilter.setResonance(resonance);
    nlFilter.reset();

    lxx::dsp::OversampledSVF osFilter(
        static_cast<float>(sampleRate),
        osFactor == 4 ? lxx::dsp::OversampledSVF::Quality::High
                      : lxx::dsp::OversampledSVF::Quality::Standard);
    osFilter.setAuthenticMode(true);
    osFilter.setCutoffFreq(fc);
    osFilter.setResonance(resonance);
    osFilter.reset();

    for (size_t i = 0; i < numFrames; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        const float in = 0.15f * std::sin(2.0f * 3.14159265359f * fc * t);
        if (useOversampled) {
            out[i] = osFilter.process(in)[1];
        } else {
            out[i] = nlFilter.process(in)[1];
        }
    }

    char path[256];
    snprintf(path, sizeof(path), "test_osf_%s_fc%.0f_res%.3f.csv", tag, fc, resonance);
    writeCsv(path, out.data(), numFrames);
    printf("  %s: settled peak=%.6f -> %s\n", tag, settledPeak(out.data(), numFrames, sampleRate), path);
}

static void qSubsetTest() {
    printf("\n=== Q subset (NL disabled, low-level sine, oversample round-trip) ===\n");
    constexpr int sr = 48000;
    constexpr size_t n = sr * 2;
    const float points[][3] = {
        {100.0f, 0.5f, 2.0f},
        {1000.0f, 0.5f, 2.0f},
        {10000.0f, 0.9f, 10.0f},
        {20000.0f, 0.9f, 10.0f},
    };

    lxx::dsp::SVFNonlinearCalibration noNl;
    noNl.enableInputSaturation = false;
    noNl.enableIntegratorSaturation = false;
    noNl.enableResonanceNonlinearity = false;

    for (const auto& p : points) {
        const float fc = p[0];
        const float res = p[1];
        const float qTarget = p[2];

        std::vector<float> direct(n), os2(n);
        lxx::dsp::SVFAnushriNonlinear directF(static_cast<float>(sr));
        directF.setCalibration(noNl);
        directF.setCutoffFreq(fc);
        directF.setResonance(res);
        directF.reset();

        lxx::dsp::OversampledSVF osF(static_cast<float>(sr));
        osF.setNonlinearCalibration(noNl);
        osF.setCutoffFreq(fc);
        osF.setResonance(res);
        osF.reset();

        float pkNative96 = 0.0f;
        if (fc >= 5000.0f) {
            lxx::dsp::SVFAnushriNonlinear f96(96000.0f);
            f96.setCalibration(noNl);
            f96.setCutoffFreq(fc);
            f96.setResonance(res);
            f96.reset();
            for (size_t j = 0; j < n * 2; ++j) {
                const float t96 = static_cast<float>(j) / 96000.0f;
                const float in96 = 0.01f * std::sin(2.0f * 3.14159265359f * fc * t96);
                const float bp96 = f96.process(in96)[1];
                pkNative96 = std::max(pkNative96, std::abs(bp96));
            }
        }

        for (size_t i = 0; i < n; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(sr);
            const float in = 0.01f * std::sin(2.0f * 3.14159265359f * fc * t);
            direct[i] = directF.process(in)[1];
            os2[i] = osF.process(in)[1];
        }

        const float pkDirect = settledPeak(direct.data(), n, sr);
        const float pkOs = settledPeak(os2.data(), n, sr);
        const float errPct = (pkDirect > 1e-9f)
            ? std::abs(pkOs - pkDirect) / pkDirect * 100.0f
            : 0.0f;
        printf("  fc=%.0f res=%.1f Q_tgt=%.1f | direct=%.6f os2x=%.6f err=%.2f%%\n",
               fc, res, qTarget, pkDirect, pkOs, errPct);

        char qpath[256];
        snprintf(qpath, sizeof(qpath), "test_osf_q_fc%.0f_res%.3f.csv", fc, res);
        FILE* qf = fopen(qpath, "w");
        if (qf) {
            fprintf(qf, "path,value\n");
            fprintf(qf, "direct,%.10f\n", pkDirect);
            fprintf(qf, "os2x,%.10f\n", pkOs);
            if (fc >= 5000.0f) {
                fprintf(qf, "native96k,%.10f\n", pkNative96);
            }
            fclose(qf);
        }
    }
}

static void selfOscTransition() {
    printf("\n=== Self-osc transition (impulse, fc=1kHz) ===\n");
    constexpr int sr = 48000;
    constexpr size_t n = static_cast<size_t>(sr * 3);
    const float resLevels[] = {0.99f, 1.0f};

    for (float res : resLevels) {
        std::vector<float> base(n), os2(n), os4(n);

        lxx::dsp::SVFAnushriNonlinear nl(static_cast<float>(sr));
        nl.setAuthenticMode(true);
        nl.setCutoffFreq(1000.0f);
        nl.setResonance(res);
        nl.reset();

        lxx::dsp::OversampledSVF os2f(static_cast<float>(sr));
        os2f.setAuthenticMode(true);
        os2f.setCutoffFreq(1000.0f);
        os2f.setResonance(res);
        os2f.reset();

        lxx::dsp::OversampledSVF os4f(
            static_cast<float>(sr), lxx::dsp::OversampledSVF::Quality::High);
        os4f.setAuthenticMode(true);
        os4f.setCutoffFreq(1000.0f);
        os4f.setResonance(res);
        os4f.reset();

        for (size_t i = 0; i < n; ++i) {
            const float in = (i == 0) ? 1.0f : 0.0f;
            base[i] = nl.process(in)[1];
            os2[i] = os2f.process(in)[1];
            os4[i] = os4f.process(in)[1];
        }

        printf("  res=%.3f | base=%.6f | 2x=%.6f | 4x=%.6f\n",
               res,
               settledPeak(base.data(), n, sr),
               settledPeak(os2.data(), n, sr),
               settledPeak(os4.data(), n, sr));

        char spath[256];
        snprintf(spath, sizeof(spath), "test_osf_selfosc_res%.3f.csv", res);
        FILE* sf = fopen(spath, "w");
        if (sf) {
            fprintf(sf, "path,value\n");
            fprintf(sf, "base,%.10f\n", settledPeak(base.data(), n, sr));
            fprintf(sf, "os2x,%.10f\n", settledPeak(os2.data(), n, sr));
            fprintf(sf, "os4x,%.10f\n", settledPeak(os4.data(), n, sr));
            fclose(sf);
        }
    }
}

int main() {
    constexpr int sr = 48000;
    constexpr size_t n = sr * 2;

    printf("=== Oversampled Filter Validation ===\n");

    printf("\n=== Aliasing worst-case (res=0.95, fc=20kHz, sine drive) ===\n");
    renderSineDrive("base_1x", 20000.0f, 0.95f, n, sr, false, 1);
    renderSineDrive("os2x", 20000.0f, 0.95f, n, sr, true, 2);
    renderSineDrive("os4x", 20000.0f, 0.95f, n, sr, true, 4);

    printf("\n=== Aliasing high-res (res=0.99, fc=1kHz, sine drive) ===\n");
    renderSineDrive("base_1x", 1000.0f, 0.99f, n, sr, false, 1);
    renderSineDrive("os2x", 1000.0f, 0.99f, n, sr, true, 2);
    renderSineDrive("os4x", 1000.0f, 0.99f, n, sr, true, 4);

    qSubsetTest();
    selfOscTransition();

    printf("\nAll oversampled filter tests complete. CSV in build directory.\n");
    return 0;
}
