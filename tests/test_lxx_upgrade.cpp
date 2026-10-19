#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "core/dc_blocker.h"
#include "drums/drum_reconstruction_filter.h"
#include "drums/native_extended_voices.h"
#include "engines/engine_bus.h"
#include "sequencer/dnb_extended_sequencer.h"
#include "sequencer/dnb_sequencer.h"
#include "sequencer/generator_common.h"
#include "sequencer/grids_sequencer.h"
#include "sequencer/midi_drum_output.h"
#include "sequencer/techno_extended_sequencer.h"
#include "sequencer/techno_sequencer.h"

using namespace lxx::dsp;

static int failures = 0;
static void expect(bool cond, const char* msg)
{
    if (!cond) { std::printf("FAIL: %s\n", msg); ++failures; }
    else std::printf("PASS: %s\n", msg);
}

static bool allFinite(const float* x, size_t n)
{
    for (size_t i = 0; i < n; ++i) if (!std::isfinite(x[i])) return false;
    return true;
}

static HostClockInput makeHost(double bpm, double ppq)
{
    HostClockInput h;
    h.enabled = true; h.isPlaying = true; h.hasPpq = true;
    h.bpm = bpm; h.sampleRate = 48000.0; h.ppq = ppq;
    return h;
}

static void collectSteps(ISequencerMode& seq, SequencerModeParams p, double bars,
                         std::vector<DrumEvent>& out, double bpm = 128.0)
{
    seq.prepare(48000.0);
    seq.setParams(p);
    seq.setRunning(true);
    const uint32_t total = static_cast<uint32_t>(bars * 4.0 * 60.0 / bpm * 48000.0);
    uint32_t done = 0;
    HostClockInput host = makeHost(bpm, 0.0);
    DrumEventBuffer buf;
    while (done < total) {
        const uint32_t n = std::min<uint32_t>(2048, total - done);
        host.ppq = (static_cast<double>(done) / 48000.0) * (bpm / 60.0);
        buf.clear();
        seq.process(n, host, buf);
        for (uint32_t i = 0; i < buf.count; ++i)
            out.push_back(buf.events[i]);
        done += n;
    }
}

/** Per-block exclusivity — offsets are chunk-relative. */
static bool hatExclusiveInBuffers(ISequencerMode& seq, SequencerModeParams p, double bars, double bpm)
{
    seq.prepare(48000.0);
    seq.setParams(p);
    seq.setRunning(true);
    const uint32_t total = static_cast<uint32_t>(bars * 4.0 * 60.0 / bpm * 48000.0);
    uint32_t done = 0;
    HostClockInput host = makeHost(bpm, 0.0);
    DrumEventBuffer buf;
    while (done < total) {
        const uint32_t n = std::min<uint32_t>(2048, total - done);
        host.ppq = (static_cast<double>(done) / 48000.0) * (bpm / 60.0);
        buf.clear();
        seq.process(n, host, buf);
        for (uint32_t i = 0; i < buf.count; ++i) {
            for (uint32_t j = i + 1; j < buf.count; ++j) {
                if (buf.events[i].sampleOffset != buf.events[j].sampleOffset)
                    continue;
                const bool aCH = buf.events[i].voice == DrumVoice::hat;
                const bool aOH = buf.events[i].voice == DrumVoice::openHat;
                const bool bCH = buf.events[j].voice == DrumVoice::hat;
                const bool bOH = buf.events[j].voice == DrumVoice::openHat;
                if ((aCH && bOH) || (aOH && bCH))
                    return false;
            }
        }
        done += n;
    }
    return true;
}

static void testNativeSixVoiceRates()
{
    const float rates[] = { 44100.f, 48000.f, 88200.f, 96000.f, 192000.f };
    for (float sr : rates) {
        DrumEngineBus bus;
        bus.init(sr, 1024);
        ExtendedVoiceParams ep;
        ep.lowMode = 1; // boom
        bus.setExtendedParams(ep);
        bus.trigger(DrumVoice::kick, 1.f);
        bus.trigger(DrumVoice::snare, 0.9f);
        bus.trigger(DrumVoice::hat, 0.7f);
        bus.trigger(DrumVoice::openHat, 0.7f, LowArticulation::boom);
        bus.trigger(DrumVoice::lowTom, 0.8f, LowArticulation::boom);
        bus.trigger(DrumVoice::midPerc, 0.7f);
        std::vector<float> buf(1024, 0.f);
        bus.processBlock(buf.data(), 1024);
        expect(allFinite(buf.data(), buf.size()), "native finite");
        float peak = 0.f;
        for (float s : buf) peak = std::max(peak, std::fabs(s));
        expect(peak > 1e-3f && peak < 8.f, "native usable level");
    }
}

static void testBoomVsTomDistinct()
{
    NativeExtendedVoices boom, tom;
    boom.init(48000.f);
    tom.init(48000.f);
    ExtendedVoiceParams p;
    boom.setParams(p);
    tom.setParams(p);
    boom.triggerLow(1.f, LowArticulation::boom);
    tom.triggerLow(1.f, LowArticulation::tom);
    std::vector<float> a(2048, 0.f), b(2048, 0.f);
    boom.processAdd(a.data(), 2048);
    tom.processAdd(b.data(), 2048);
    expect(allFinite(a.data(), a.size()) && allFinite(b.data(), b.size()), "boom/tom finite");
    double diff = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
        diff += std::fabs(static_cast<double>(a[i] - b[i]));
    expect(diff > 1.0, "boom and tom structurally distinct");
}

static void testOpenHatChoke()
{
    DrumEngineBus bus;
    bus.init(48000.f, 512);
    bus.trigger(DrumVoice::openHat, 1.f);
    std::vector<float> buf(512, 0.f);
    bus.processBlock(buf.data(), 256);
    bus.trigger(DrumVoice::hat, 1.f); // must choke OH
    bus.processBlock(buf.data(), 256);
    expect(allFinite(buf.data(), buf.size()), "choke finite");
}

static void testTechnoGravity()
{
    TechnoSequencerMode t;
    SequencerModeParams p;
    p.mode = SequencerModeId::techno;
    p.technoSeed = 42;
    p.technoDensity = 0.5f;
    p.tempo = 128;
    std::vector<DrumEvent> ev;
    collectSteps(t, p, 4.0, ev);
    int kickTotal = 0;
    for (auto& e : ev)
        if (e.voice == DrumVoice::kick) ++kickTotal;
    expect(kickTotal >= 12, "techno kick gravity across 4 bars");
    expect(hatExclusiveInBuffers(t, p, 4.0, 128.0), "techno CH/OH exclusive");
}

static void testTechnoExtSixVoices()
{
    TechnoExtendedSequencerMode ext;
    SequencerModeParams p;
    p.mode = SequencerModeId::technoExtended;
    p.technoSeed = 1847;
    p.technoDensity = 0.65f;
    p.technoSyncopation = 0.35f;
    p.technoFill = 0.45f;
    p.tempo = 128;
    std::vector<DrumEvent> ev;
    collectSteps(ext, p, 16.0, ev);

    bool saw[6] {};
    int low = 0, kick = 0, mid = 0, oh = 0;
    int boom = 0, tom = 0;
    for (auto& e : ev) {
        const int note = midiNoteForVoice(e.voice);
        if (note == 35) { saw[0] = true; ++low; if (e.lowArticulation == LowArticulation::boom) ++boom; else ++tom; }
        if (note == 36) { saw[1] = true; ++kick; }
        if (note == 38) saw[2] = true;
        if (note == 39) { saw[3] = true; ++mid; }
        if (note == 42) saw[4] = true;
        if (note == 46) { saw[5] = true; ++oh; }
    }
    expect(saw[0] && saw[1] && saw[2] && saw[3] && saw[4] && saw[5],
           "techno-ext emits 35/36/38/39/42/46");
    expect(low > 4 && low < kick, "techno-ext low active and bounded");
    expect(mid > 0 && oh > 0, "techno-ext mid and OH");
    expect(boom > 0 && tom > 0, "techno-ext boom and tom articulations");
    expect(hatExclusiveInBuffers(ext, p, 8.0, 128.0), "techno-ext CH/OH exclusivity");
}

static void testDnb()
{
    DnbSequencerMode d;
    SequencerModeParams p;
    p.mode = SequencerModeId::dnb;
    p.technoSeed = 99;
    p.technoDensity = 0.55f;
    p.technoSyncopation = 0.4f;
    p.tempo = 170;
    std::vector<DrumEvent> ev;
    collectSteps(d, p, 8.0, ev, 170.0);
    int kickTotal = 0, snare = 0;
    for (auto& e : ev) {
        if (e.voice == DrumVoice::kick) ++kickTotal;
        if (e.voice == DrumVoice::snare) ++snare;
    }
    expect(kickTotal > 5, "dnb has kicks");
    expect(snare > 3, "dnb has snares");
    expect(kickTotal < 40, "dnb not quarter-note dominated");
    expect(hatExclusiveInBuffers(d, p, 8.0, 170.0), "dnb CH/OH exclusive");
}

static void testDnbExtSixVoices()
{
    DnbExtendedSequencerMode ext;
    SequencerModeParams p;
    p.mode = SequencerModeId::dnbExtended;
    p.technoSeed = 777;
    p.technoDensity = 0.7f;
    p.technoSyncopation = 0.5f;
    p.technoFill = 0.4f;
    p.tempo = 170;
    std::vector<DrumEvent> ev;
    collectSteps(ext, p, 16.0, ev, 170.0);
    bool saw[6] {};
    int boom = 0, tom = 0;
    for (auto& e : ev) {
        const int note = midiNoteForVoice(e.voice);
        if (note == 35) { saw[0] = true; if (e.lowArticulation == LowArticulation::boom) ++boom; else ++tom; }
        if (note == 36) saw[1] = true;
        if (note == 38) saw[2] = true;
        if (note == 39) saw[3] = true;
        if (note == 42) saw[4] = true;
        if (note == 46) saw[5] = true;
    }
    expect(saw[0] && saw[1] && saw[2] && saw[3] && saw[4] && saw[5],
           "dnb-ext emits 35/36/38/39/42/46");
    expect(boom + tom > 0, "dnb-ext low articulations");
    expect(hatExclusiveInBuffers(ext, p, 8.0, 170.0), "dnb-ext CH/OH exclusivity");
}

static void testSeqOnly()
{
    DrumEngineBus bus;
    bus.init(48000.f, 512);
    bus.setSequencerOnly(true);
    bus.trigger(DrumVoice::kick, 1.f);
    std::vector<float> buf(512, 1.f);
    const auto before = bus.bypassedRenderCount();
    bus.processBlock(buf.data(), 512);
    expect(bus.bypassedRenderCount() == before + 1, "seqonly bypass");
    float peak = 0.f;
    for (float s : buf) peak = std::max(peak, std::fabs(s));
    expect(peak < 1e-8f, "seqonly silence");
}

static void testMidiMap()
{
    expect(midiNoteForVoice(DrumVoice::lowTom) == 35, "B0=35");
    expect(midiNoteForVoice(DrumVoice::kick) == 36, "C1=36");
    expect(midiNoteForVoice(DrumVoice::snare) == 38, "D1=38");
    expect(midiNoteForVoice(DrumVoice::midPerc) == 39, "D#1=39");
    expect(midiNoteForVoice(DrumVoice::hat) == 42, "F#1=42");
    expect(midiNoteForVoice(DrumVoice::openHat) == 46, "A#1=46");
}

static void testHatExclusivityHelper()
{
    DrumEventBuffer out;
    emitEvent(out, DrumVoice::hat, 10, 0.5f, false);
    emitEvent(out, DrumVoice::openHat, 10, 0.7f, false);
    enforceHatExclusivityPass(out, 0);
    bool hasCH = false, hasOH = false;
    for (uint32_t i = 0; i < out.count; ++i) {
        if (out.events[i].sampleOffset != 10) continue;
        if (out.events[i].voice == DrumVoice::hat) hasCH = true;
        if (out.events[i].voice == DrumVoice::openHat) hasOH = true;
    }
    expect(hasOH && !hasCH, "OH wins exclusivity pass");
}

static void testMixerVolumeAndMute()
{
    expect(std::fabs(dbToLinearGain(0.0f) - 1.0f) < 1e-5f, "0 dB = unity");
    expect(dbToLinearGain(-60.0f) == 0.0f, "-60 dB silent");
    expect(dbToLinearGain(6.0f) > 1.5f, "+6 dB boost");

    DrumEngineBus bus;
    bus.init(48000.f, 2048);
    VoiceMixerParams m {};
    bus.setMixerParams(m);
    bus.trigger(DrumVoice::kick, 1.f);
    std::vector<float> a(2048, 0.f);
    bus.processBlock(a.data(), 2048);
    float peak0 = 0.f;
    for (float s : a) peak0 = std::max(peak0, std::fabs(s));
    expect(peak0 > 1e-3f, "0 dB kick audible");

    DrumEngineBus bus2;
    bus2.init(48000.f, 2048);
    m.volDb[0] = -24.0f;
    bus2.setMixerParams(m);
    bus2.trigger(DrumVoice::kick, 1.f);
    std::vector<float> b(2048, 0.f);
    // settle smoothers
    for (int i = 0; i < 8; ++i)
        bus2.processBlock(b.data(), 2048);
    bus2.trigger(DrumVoice::kick, 1.f);
    std::fill(b.begin(), b.end(), 0.f);
    bus2.processBlock(b.data(), 2048);
    float peakAtt = 0.f;
    for (float s : b) peakAtt = std::max(peakAtt, std::fabs(s));
    expect(peakAtt < peak0 * 0.5f, "attenuation reduces kick");

    DrumEngineBus bus3;
    bus3.init(48000.f, 2048);
    m.volDb[0] = 0.0f;
    m.mute[0] = true;
    bus3.setMixerParams(m);
    for (int i = 0; i < 8; ++i)
        bus3.processBlock(b.data(), 2048);
    bus3.trigger(DrumVoice::kick, 1.f);
    std::fill(b.begin(), b.end(), 0.f);
    bus3.processBlock(b.data(), 2048);
    float peakMute = 0.f;
    for (float s : b) peakMute = std::max(peakMute, std::fabs(s));
    expect(peakMute < 1e-4f, "muted kick silent");
}

static void testMutedChStillChokesOh()
{
    DrumEngineBus bus;
    bus.init(48000.f, 512);
    VoiceMixerParams m {};
    m.mute[2] = true; // mute CH audio
    bus.setMixerParams(m);
    for (int i = 0; i < 8; ++i) {
        std::vector<float> z(512, 0.f);
        bus.processBlock(z.data(), 512);
    }
    bus.trigger(DrumVoice::openHat, 1.f);
    std::vector<float> oh(512, 0.f);
    bus.processBlock(oh.data(), 512);
    float peakOh = 0.f;
    for (float s : oh) peakOh = std::max(peakOh, std::fabs(s));
    expect(peakOh > 1e-4f, "OH audible before choke");

    bus.trigger(DrumVoice::hat, 1.f); // muted CH still chokes
    std::fill(oh.begin(), oh.end(), 0.f);
    bus.processBlock(oh.data(), 512);
    float after = 0.f;
    for (float s : oh) after = std::max(after, std::fabs(s));
    expect(after < peakOh * 0.35f, "muted CH still chokes OH");
}

static float rmsOf(const float* x, size_t n)
{
    double a = 0.0;
    for (size_t i = 0; i < n; ++i) a += static_cast<double>(x[i]) * x[i];
    return static_cast<float>(std::sqrt(a / static_cast<double>(n)));
}

static float meanOf(const float* x, size_t n)
{
    double a = 0.0;
    for (size_t i = 0; i < n; ++i) a += x[i];
    return static_cast<float>(a / static_cast<double>(n));
}

static float peakOf(const float* x, size_t n)
{
    float p = 0.f;
    for (size_t i = 0; i < n; ++i) p = std::max(p, std::fabs(x[i]));
    return p;
}

static void testIdleSilence()
{
    const float rates[] = { 44100.f, 48000.f, 96000.f };
    for (float sr : rates) {
        DrumEngineBus bus;
        bus.init(sr, 2048);
        bus.reset();
        std::vector<float> buf(2048, 1.f);
        for (int i = 0; i < 16; ++i)
            bus.processBlock(buf.data(), 2048);
        expect(peakOf(buf.data(), buf.size()) < 1e-6f, "idle peak silent");
        expect(rmsOf(buf.data(), buf.size()) < 1e-7f, "idle RMS silent");
        expect(std::fabs(meanOf(buf.data(), buf.size())) < 1e-7f, "idle DC silent");
    }
}

static void testVoiceTailSilence()
{
    const DrumVoice voices[] = {
        DrumVoice::kick, DrumVoice::snare, DrumVoice::hat,
        DrumVoice::lowTom, DrumVoice::midPerc, DrumVoice::openHat
    };
    for (auto v : voices) {
        DrumEngineBus bus;
        bus.init(48000.f, 2048);
        ExtendedVoiceParams ep;
        ep.lowMode = 1;
        bus.setExtendedParams(ep);
        bus.trigger(v, 1.f, LowArticulation::boom);
        std::vector<float> buf(2048, 0.f);
        // Drain for ~2 seconds
        for (int i = 0; i < 50; ++i)
            bus.processBlock(buf.data(), 2048);
        expect(peakOf(buf.data(), buf.size()) < 5e-4f, "voice tail near silent");
        expect(std::fabs(meanOf(buf.data(), buf.size())) < 1e-4f, "voice tail low DC");
    }
}

static void testReconstructionCutsUltrasonics()
{
    // Nyquist-rate square (PWM/ZOH images) — reconstruction must crush it.
    DrumReconstructionFilter f;
    f.prepare(48000.f);
    std::vector<float> raw(8192), filt(8192);
    for (size_t i = 0; i < raw.size(); ++i)
        raw[i] = (i & 1) ? 0.5f : -0.5f;
    filt = raw;
    f.processBlock(filt.data(), static_cast<uint32_t>(filt.size()));
    const float peakRaw = peakOf(raw.data(), raw.size());
    const float rmsFilt = rmsOf(filt.data() + 2000, filt.size() - 2000);
    const float rmsRaw = rmsOf(raw.data() + 2000, raw.size() - 2000);
    expect(rmsFilt < rmsRaw * 0.45f, "reconstruction attenuates Nyquist ZOH");
    expect(peakRaw > 0.4f, "nyquist stimulus present");
    expect(allFinite(filt.data(), filt.size()), "reconstruction finite");
}

static void testBandwidthNeutralVsCrush()
{
    DrumEngineBus openBus, crushBus;
    openBus.init(48000.f, 2048);
    crushBus.init(48000.f, 2048);
    LegacyEngineParams openP, crushP;
    openP.bandwidth = 255;
    openP.extended = true;
    crushP.bandwidth = 32;
    crushP.extended = true;
    openBus.setLegacyParams(openP);
    crushBus.setLegacyParams(crushP);
    openBus.trigger(DrumVoice::kick, 1.f);
    crushBus.trigger(DrumVoice::kick, 1.f);
    std::vector<float> a(2048, 0.f), b(2048, 0.f);
    openBus.processBlock(a.data(), 2048);
    crushBus.processBlock(b.data(), 2048);
    expect(allFinite(a.data(), a.size()) && allFinite(b.data(), b.size()), "bandwidth finite");
    double diff = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
        diff += std::fabs(static_cast<double>(a[i] - b[i]));
    expect(diff > 1e-4, "bandwidth changes output");
}

static void testDcBlockerSampleRates()
{
    const float rates[] = { 44100.f, 48000.f, 96000.f, 192000.f };
    for (float sr : rates) {
        DcBlocker dc;
        dc.prepare(sr);
        float y = 0.f;
        // ~3 s settle — pole ~0.9999 @ 44.1k needs many samples
        const int n = static_cast<int>(sr * 3.0f);
        for (int i = 0; i < n; ++i)
            y = dc.process(0.25f);
        expect(std::fabs(y) < 0.02f, "DC blocker settles constant");
        dc.reset();
        float z = 0.f;
        for (int i = 0; i < 1000; ++i)
            z = std::max(z, std::fabs(dc.process(0.0f)));
        expect(z < 1e-8f, "DC blocker silence");
    }
}

static void testKickDcNegligible()
{
    DrumEngineBus bus;
    bus.init(48000.f, 4096);
    bus.trigger(DrumVoice::kick, 1.f);
    std::vector<float> buf(4096, 0.f);
    bus.processBlock(buf.data(), 4096);
    // Mean over hit should not be a large bias (boom may have LF but not pure DC)
    expect(std::fabs(meanOf(buf.data(), buf.size())) < 0.08f, "kick mean not large DC");
    expect(peakOf(buf.data(), buf.size()) > 1e-3f, "kick audible");
}

int main()
{
    testNativeSixVoiceRates();
    testBoomVsTomDistinct();
    testOpenHatChoke();
    testMixerVolumeAndMute();
    testMutedChStillChokesOh();
    testIdleSilence();
    testVoiceTailSilence();
    testReconstructionCutsUltrasonics();
    testBandwidthNeutralVsCrush();
    testDcBlockerSampleRates();
    testKickDcNegligible();
    testTechnoGravity();
    testTechnoExtSixVoices();
    testDnb();
    testDnbExtSixVoices();
    testSeqOnly();
    testMidiMap();
    testHatExclusivityHelper();
    std::printf("failures=%d\n", failures);
    return failures == 0 ? 0 : 1;
}
