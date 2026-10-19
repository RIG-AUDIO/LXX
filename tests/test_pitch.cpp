#include "voice/voice.h"
#include "oscillators/blep_oscillator.h"
#include "core/dsp_types.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

int failures = 0;

void expectTrue(bool cond, const char* msg)
{
    if (!cond) {
        std::printf("FAIL: %s\n", msg);
        ++failures;
    }
}

float midiHz(int note)
{
    return 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
}

float dftPeakHz(const float* x, size_t n, float sampleRate, float minHz, float maxHz)
{
    const int i0 = std::max(1, static_cast<int>(minHz * static_cast<float>(n) / sampleRate));
    const int i1 = std::min(static_cast<int>(n / 2) - 1,
                            static_cast<int>(maxHz * static_cast<float>(n) / sampleRate));
    double best = 0.0;
    int bestBin = i0;
    for (int k = i0; k <= i1; ++k) {
        double re = 0.0, im = 0.0;
        const double w = -2.0 * 3.14159265358979323846 * static_cast<double>(k)
                         / static_cast<double>(n);
        for (size_t i = 0; i < n; ++i) {
            const double a = w * static_cast<double>(i);
            re += static_cast<double>(x[i]) * std::cos(a);
            im += static_cast<double>(x[i]) * std::sin(a);
        }
        const double mag = re * re + im * im;
        if (mag > best) {
            best = mag;
            bestBin = k;
        }
    }
    return static_cast<float>(bestBin) * sampleRate / static_cast<float>(n);
}

lxx::dsp::VoicePatch trackingPatch()
{
    lxx::dsp::VoicePatch p;
    p.authenticMode = false;
    p.subLevel = 0.0f;
    p.waveLevel = 1.0f;
    p.filterCutoffHz = 18000.0f;
    p.filterResonance = 0.0f;
    p.filterKbdTrack = 0.0f;
    p.filterEnvAmt = 0.0f;
    p.envAttack = 0.001f;
    p.envDecay = 0.2f;
    p.envSustain = 1.0f;
    p.envRelease = 0.05f;
    p.envVcaMorph = 0.5f;
    p.glideTimeSec = 0.0f;
    p.unisonCount = 1;
    p.pitchEnvAmt = 0.0f;
    p.pitchLfoAmt = 0.0f;
    p.oscRange = 0.0f;
    p.oscDcoFine = 0.0f;
    p.oscDetune = 0.0f;
    p.oscFine = 0.0f;
    return p;
}

float centsError(float expected, float actual)
{
    if (expected <= 0.0f || actual <= 0.0f)
        return 1.0e6f;
    return 1200.0f * std::log2(actual / expected);
}

void testBlepDirect()
{
    std::printf("BLEP oscillator A4 at 44.1/48/96 kHz\n");
    const float rates[] = { 44100.0f, 48000.0f, 88200.0f, 96000.0f };
    for (float sr : rates) {
        lxx::dsp::BlePOscillator osc(sr);
        osc.setWaveform(lxx::dsp::Waveform::SAWTOOTH);
        osc.setFrequency(440.0f);
        const int n = 4096;
        std::vector<float> buf(static_cast<size_t>(n));
        osc.processBlock(buf.data(), buf.size());
        const float peak = dftPeakHz(buf.data(), buf.size(), sr, 200.0f, 800.0f);
        const float err = centsError(440.0f, peak);
        std::printf("  sr=%.0f peak=%.2f err=%.1fc\n", sr, peak, err);
        expectTrue(std::abs(err) < 40.0f, "BLEP A4 within 40 cents");
    }
}

void testVoiceMatrix()
{
    std::printf("MIDI NOTE  EXPECTED  INTERNAL  AUDIO  ERROR_CENTS\n");
    const int notes[] = { 36, 48, 60, 69, 72, 84 };
    const float rates[] = { 44100.0f, 48000.0f, 88200.0f, 96000.0f };
    float f60[4] = {};
    int rateIndex = 0;
    for (float sr : rates) {
        std::printf("sample rate %.0f\n", sr);
        for (int note : notes) {
            lxx::dsp::Voice voice;
            voice.init(sr, 2048);
            voice.setPatch(trackingPatch());
            voice.noteOn(static_cast<uint8_t>(note), 100);
            const int n = 8192;
            std::vector<float> buf(static_cast<size_t>(n), 0.0f);
            size_t filled = 0;
            while (filled < buf.size()) {
                const size_t chunk = std::min<size_t>(64, buf.size() - filled);
                voice.processBlock(buf.data() + filled, chunk);
                filled += chunk;
            }
            const float expected = midiHz(note);
            const float internal = voice.lastOscillatorFrequencyHz();
            const float lo = expected * 0.5f;
            const float hi = expected * 1.8f;
            const float audio = dftPeakHz(buf.data() + 2048, 4096, sr, lo, hi);
            const float errI = centsError(expected, internal);
            const float errA = centsError(expected, audio);
            std::printf("  MIDI %3d  %8.2f  %8.2f  %8.2f  int=%5.1f audio=%5.1f\n",
                        note, expected, internal, audio, errI, errA);
            expectTrue(std::abs(errI) < 5.0f, "internal frequency tracks MIDI");
            expectTrue(std::abs(errA) < 130.0f, "audio fundamental near MIDI (DFT bin limited)");
            expectTrue(voice.lastMidiNote() == static_cast<uint8_t>(note), "note stored");
            if (note == 60)
                f60[rateIndex] = audio;
        }
        ++rateIndex;
    }

    std::printf("Octaves @ 48 kHz\n");
    lxx::dsp::Voice voice;
    voice.init(48000.0f, 2048);
    voice.setPatch(trackingPatch());
    auto renderNote = [&](uint8_t note) {
        voice.allSoundOff();
        voice.noteOn(note, 100);
        std::vector<float> buf(8192, 0.0f);
        for (size_t i = 0; i < buf.size(); i += 64)
            voice.processBlock(buf.data() + i, 64);
        return voice.lastOscillatorFrequencyHz();
    };
    const float c4 = renderNote(60);
    const float c5 = renderNote(72);
    const float c3 = renderNote(48);
    std::printf("  60=%.2f 72=%.2f (2x=%.2f) 48=%.2f (0.5x=%.2f)\n",
                c4, c5, 2.0f * c4, c3, 0.5f * c4);
    expectTrue(std::abs(c5 / c4 - 2.0f) < 0.02f, "MIDI 72 is octave above 60");
    expectTrue(std::abs(c3 / c4 - 0.5f) < 0.02f, "MIDI 48 is octave below 60");

    for (int i = 1; i < 4; ++i) {
        const float err = centsError(f60[0], f60[i]);
        expectTrue(std::abs(err) < 130.0f, "MIDI 60 audio pitch sample-rate invariant");
    }
}

void testBendLegatoRetrigger()
{
    lxx::dsp::Voice voice;
    voice.init(48000.0f, 2048);
    auto p = trackingPatch();
    voice.setPatch(p);
    std::vector<float> buf(64);

    voice.noteOn(60, 100);
    voice.processBlock(buf.data(), buf.size());
    const float base = voice.lastOscillatorFrequencyHz();

    voice.setPitchBend(2.0f);
    voice.processBlock(buf.data(), buf.size());
    const float up = voice.lastOscillatorFrequencyHz();
    expectTrue(std::abs(centsError(base * std::pow(2.0f, 2.0f / 12.0f), up)) < 15.0f,
               "pitch bend +2 semitones");

    voice.setPitchBend(-2.0f);
    voice.processBlock(buf.data(), buf.size());
    const float down = voice.lastOscillatorFrequencyHz();
    expectTrue(std::abs(centsError(base * std::pow(2.0f, -2.0f / 12.0f), down)) < 15.0f,
               "pitch bend -2 semitones");

    voice.setPitchBend(0.0f);
    voice.noteOn(64, 100);
    voice.processBlock(buf.data(), buf.size());
    const float e4 = voice.lastOscillatorFrequencyHz();
    expectTrue(std::abs(centsError(midiHz(64), e4)) < 8.0f, "legato overlap tracks new note");

    voice.noteOff(64);
    voice.processBlock(buf.data(), buf.size());
    const float back = voice.lastOscillatorFrequencyHz();
    expectTrue(std::abs(centsError(midiHz(60), back)) < 8.0f, "release fallback to held note");

    voice.noteOff(60);
    voice.noteOn(67, 100);
    voice.processBlock(buf.data(), buf.size());
    voice.noteOff(67);
    voice.noteOn(67, 110);
    voice.processBlock(buf.data(), buf.size());
    expectTrue(std::abs(centsError(midiHz(67), voice.lastOscillatorFrequencyHz())) < 8.0f,
               "same-note retrigger");
}

} // namespace

int main()
{
    testBlepDirect();
    testVoiceMatrix();
    testBendLegatoRetrigger();
    if (failures == 0)
        std::printf("test_pitch: PASS\n");
    else
        std::printf("test_pitch: FAIL (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}
