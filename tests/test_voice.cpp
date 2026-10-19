#include "../src/dsp/voice/voice.h"
#include <cstdio>
#include <vector>
#include <cmath>
#include <algorithm>
#include <string>

void writeWav(const char* filename, const float* data, size_t numFrames, int sampleRate = 48000) {
    FILE* f = fopen(filename, "wb");
    if (!f) return;
    const uint32_t dataSize = static_cast<uint32_t>(numFrames * sizeof(float));
    const uint32_t fileSize = 36 + dataSize;
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
    const uint32_t byteRate = sampleRate * static_cast<uint32_t>(sizeof(float));
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

struct AudioStats {
    float peak = 0.0f;
    float rms = 0.0f;
    float attackRms = 0.0f;
    float sustainRms = 0.0f;
    float zeroCrossRate = 0.0f;
};

AudioStats analyze(const float* data, size_t n, int sampleRate) {
    AudioStats s;
    double sumSq = 0.0;
    int zc = 0;
    for (size_t i = 0; i < n; ++i) {
        s.peak = std::max(s.peak, std::abs(data[i]));
        sumSq += data[i] * data[i];
        if (i > 0 && ((data[i] >= 0) != (data[i - 1] >= 0))) {
            ++zc;
        }
    }
    s.rms = static_cast<float>(std::sqrt(sumSq / static_cast<double>(n)));
    s.zeroCrossRate = static_cast<float>(zc) / (static_cast<float>(n) / sampleRate);

    const size_t attackEnd = std::min(n, static_cast<size_t>(sampleRate * 0.05));
    const size_t sustainStart = std::min(n, static_cast<size_t>(sampleRate * 0.5));
    double aSq = 0.0, susSq = 0.0;
    for (size_t i = 0; i < attackEnd; ++i) aSq += data[i] * data[i];
    for (size_t i = sustainStart; i < n; ++i) susSq += data[i] * data[i];
    s.attackRms = static_cast<float>(std::sqrt(aSq / attackEnd));
    s.sustainRms = static_cast<float>(std::sqrt(susSq / (n - sustainStart)));
    return s;
}

void renderVoice(lxx::dsp::Voice& voice, std::vector<float>& buf, float durationSec) {
    const size_t n = static_cast<size_t>(48000.0f * durationSec);
    buf.assign(n, 0.0f);
    constexpr size_t kBlock = 512;
    for (size_t offset = 0; offset < n; offset += kBlock) {
        const size_t block = std::min(kBlock, n - offset);
        voice.processBlock(buf.data() + offset, block);
    }
}

int main() {
    using namespace lxx::dsp;
    constexpr int sr = 48000;
    int failures = 0;

    std::printf("Voice integration tests (Track A)\n");
    std::fflush(stdout);

    auto makeInitPatch = []() {
        VoicePatch p;
        p.authenticMode = true;
        p.filterCutoffHz = 4000.0f;
        p.filterResonance = 0.35f;
        p.envAttack = 0.005f;
        p.envDecay = 0.25f;
        p.envSustain = 0.6f;
        p.envRelease = 0.4f;
        p.volume = 0.85f;
        p.waveLevel = 0.75f;
        p.subLevel = 0.35f;
        return p;
    };

    // --- Test 1: Single note A4 ---
    {
        Voice voice;
        voice.init(sr);
        auto patch = makeInitPatch();
        voice.setPatch(patch);
        voice.noteOn(69, 100);

        std::vector<float> buf;
        renderVoice(voice, buf, 1.0f);
        writeWav("test_voice_single_note.wav", buf.data(), buf.size(), sr);
        auto stats = analyze(buf.data(), buf.size(), sr);
        printf("Test 1 single note (A4):\n");
        printf("  peak=%.4f rms=%.4f sustainRms=%.4f zc/s=%.1f\n",
               stats.peak, stats.rms, stats.sustainRms, stats.zeroCrossRate);
        printf("  Audio: triangle+sub through LP VCF, audible ~440 Hz tone with ADSR decay.\n");
        if (stats.sustainRms < 0.001f) {
            printf("  FAIL: near silence in sustain\n");
            ++failures;
        }
        if (stats.peak > 1.0f) {
            printf("  WARN: peak > 0 dBFS (%.3f) — check staging\n", stats.peak);
        }
        voice.allSoundOff();
    }

    // --- Test 2: Legato (no envelope retrigger) ---
    {
        Voice voice;
        voice.init(sr);
        auto patch = makeInitPatch();
        patch.envLegatoMode = true;
        patch.glideTimeSec = 0.0f;
        voice.setPatch(patch);

        std::vector<float> buf(48000 * 2);
        voice.noteOn(60, 100);
        voice.processBlock(buf.data(), 48000);
        const float peakAfterFirst = voice.getLastPeak();

        voice.noteOn(64, 100);
        voice.processBlock(buf.data() + 48000, 48000);
        const float peakLegato = voice.getLastPeak();

        writeWav("test_voice_legato.wav", buf.data(), buf.size(), sr);
        auto stats = analyze(buf.data(), buf.size(), sr);
        printf("\nTest 2 legato C4->E4:\n");
        printf("  peak1=%.4f peakLegato=%.4f overallPeak=%.4f\n",
               peakAfterFirst, peakLegato, stats.peak);
        printf("  Audio: continuous tone, pitch step at 1s without click/re-attack dip.\n");
        if (peakLegato < peakAfterFirst * 0.3f) {
            printf("  FAIL: legato retriggered envelope (large level drop)\n");
            ++failures;
        }
    }

    // --- Test 3: Glide ---
    {
        Voice voice;
        voice.init(sr);
        auto patch = makeInitPatch();
        patch.glideTimeSec = 1.5f;
        patch.envLegatoMode = true;
        voice.setPatch(patch);

        std::vector<float> buf(48000 * 2);
        voice.noteOn(48, 100);
        voice.processBlock(buf.data(), 48000);
        voice.noteOn(60, 100);
        voice.processBlock(buf.data() + 48000, 48000);

        writeWav("test_voice_glide.wav", buf.data(), buf.size(), sr);
        auto stats = analyze(buf.data(), buf.size(), sr);
        printf("\nTest 3 glide C3->C4 (1.5s):\n");
        printf("  peak=%.4f rms=%.4f\n", stats.peak, stats.rms);
        printf("  Audio: smooth pitch bend over ~1.5s after second note.\n");
        voice.allSoundOff();
    }

    // --- Test 4: Unison 4 voices ---
    {
        Voice voice;
        voice.init(sr);
        auto patch = makeInitPatch();
        patch.unisonCount = 4;
        patch.unisonDetuneCents = 18.0f;
        patch.filterResonance = 0.25f;
        voice.setPatch(patch);
        voice.noteOn(57, 100);

        std::vector<float> buf;
        renderVoice(voice, buf, 1.5f);
        writeWav("test_voice_unison4.wav", buf.data(), buf.size(), sr);
        auto stats = analyze(buf.data(), buf.size(), sr);
        printf("\nTest 4 unison x4 (A3, 18ct spread):\n");
        printf("  peak=%.4f rms=%.4f zc/s=%.1f\n", stats.peak, stats.rms, stats.zeroCrossRate);
        printf("  Audio: thicker A3 cluster (peak %.3f vs single %.3f), dense low-mid beating.\n",
               stats.peak, 0.1032f);
        voice.allSoundOff();
    }

    // --- Test 5: Filter self-osc through VCA ---
    {
        Voice voice;
        voice.init(sr);
        VoicePatch patch;
        patch.authenticMode = true;
        patch.filterCutoffHz = 440.0f;
        patch.filterResonance = 1.0f;
        patch.waveLevel = 0.0f;
        patch.subLevel = 0.0f;
        patch.envAttack = 0.001f;
        patch.envDecay = 2.0f;
        patch.envSustain = 1.0f;
        patch.envRelease = 0.5f;
        patch.volume = 0.9f;
        voice.setPatch(patch);
        voice.noteOn(69, 100);
        // Brief osc burst to excite self-oscillation (zero input cannot start SVF from rest)
        {
            auto excitePatch = patch;
            excitePatch.waveLevel = 0.4f;
            voice.setPatch(excitePatch);
            std::vector<float> ping(256, 0.0f);
            voice.processBlock(ping.data(), ping.size());
            voice.setPatch(patch);
        }

        std::vector<float> buf;
        renderVoice(voice, buf, 1.5f);
        writeWav("test_voice_selfosc_vca.wav", buf.data(), buf.size(), sr);
        auto stats = analyze(buf.data(), buf.size(), sr);
        const size_t tail = buf.size() - sr;
        float tailPeak = 0.0f;
        for (size_t i = tail; i < buf.size(); ++i) {
            tailPeak = std::max(tailPeak, std::abs(buf[i]));
        }
        printf("\nTest 5 self-osc filter -> VCA (res=1, no osc):\n");
        printf("  overallPeak=%.4f tailPeak=%.4f filterPeak=%.4f\n",
               stats.peak, tailPeak, voice.getFilterPeak());
        printf("  Audio: sine-like self-osc ~cutoff, gated by VCA envelope.\n");
        if (tailPeak < 0.01f) {
            printf("  FAIL: no sustained self-oscillation\n");
            ++failures;
        }
        if (stats.peak > 0.98f) {
            printf("  WARN: near clipping (%.3f) — staging may need trim\n", stats.peak);
        }
        voice.allSoundOff();
    }

    printf("\n%s: %d failure(s)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures;
}
