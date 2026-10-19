#include "sequencer/arpeggiator.h"
#include "sequencer/clock.h"
#include "sequencer/sequencer_resources.h"
#include "sequencer/step_sequencer.h"
#include "sequencer/transport.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expectTrue(bool condition, const char* message)
{
    if (!condition) {
        std::printf("FAIL: %s\n", message);
        ++failures;
    }
}

void expectNear(double actual, double expected, double tolerance, const char* message)
{
    if (std::fabs(actual - expected) > tolerance) {
        std::printf("FAIL: %s (actual=%.9f expected=%.9f tol=%.9f)\n",
                    message, actual, expected, tolerance);
        ++failures;
    }
}

std::vector<double> collectStepTimesFirmwareTicks(lxx::dsp::ClockEngine& clock, int maxSteps)
{
    std::vector<double> stepTimes;
    uint64_t tick = 0;
    while (static_cast<int>(stepTimes.size()) < maxSteps) {
        clock.tickFirmwareTimer();
        const uint8_t before = clock.grooveStep();
        clock.countEvents();
        if (clock.consumeStepPulse()) {
            stepTimes.push_back(static_cast<double>(tick) / lxx::dsp::kFirmwareClockHz);
            (void)before;
        }
        ++tick;
    }
    return stepTimes;
}

void testIndependentStepTiming()
{
    std::printf("Test: independent step timing at 120 BPM\n");
    lxx::dsp::ClockEngine clock;
    clock.reset();
    clock.update(120, 0, 0, 1);

    const auto times = collectStepTimesFirmwareTicks(clock, 8);
    expectTrue(times.size() == 8, "captured 8 step pulses");

    const double expected = lxx::dsp::independentStepPeriodSeconds(120);
    for (size_t i = 1; i < times.size(); ++i) {
        const double interval = times[i] - times[i - 1];
        expectNear(interval, expected, expected * 0.002, "16th-note interval matches closed-form BPM formula");
    }
}

void testSwingAlternation()
{
    std::printf("Test: swing alternates groove tick durations\n");
    lxx::dsp::ClockEngine clock;
    clock.reset();
    clock.update(120, 64, 0, 1);

    const auto& table = clock.tickDurationTable();
    expectTrue(table[0] != table[1], "swing=64 produces different tick durations on steps 0 and 1");

    const int16_t base = static_cast<int16_t>(lxx::dsp::firmwareBaseTickDuration(120));
    const int16_t* groove = lxx::dsp::grooveTemplateTable(1);
    const int32_t swingAmount = 64 >> 1;
    const int16_t expected0 = static_cast<int16_t>(base + ((groove[0] * base * swingAmount) >> 16));
    const int16_t expected1 = static_cast<int16_t>(base + ((groove[1] * base * swingAmount) >> 16));
    expectTrue(table[0] == static_cast<uint16_t>(expected0), "step 0 tick duration matches independent swing formula");
    expectTrue(table[1] == static_cast<uint16_t>(expected1), "step 1 tick duration matches independent swing formula");
}

void testArpPatternMaskCycle()
{
    std::printf("Test: arpeggiator pattern mask advances over 16 steps\n");
    lxx::dsp::Arpeggiator arp;
    lxx::dsp::SequencerSettings settings;
    settings.arpMode = 1;
    settings.arpPattern = 0;
    arp.setSettings(settings);

    lxx::dsp::NoteStack<16> keys;
    keys.noteOn(60, 100);
    arp.setPressedKeys(keys);
    arp.start();

    const uint16_t pattern = lxx::dsp::kArpeggiatorPatterns[0];
    uint16_t expectedMask = 1;
    for (int step = 0; step < 16; ++step) {
        const bool expectGate = (expectedMask & pattern) != 0;
        const auto events = arp.clockStep(static_cast<uint8_t>(step));
        expectTrue((!events.empty()) == expectGate, "arp gate matches pattern bitmask");
        expectedMask = static_cast<uint16_t>(expectedMask << 1);
        if (expectedMask == 0) {
            expectedMask = 1;
        }
    }
}

void testSequenceWrapAndSpecialNotes()
{
    std::printf("Test: 128-step sequencer wrap + rest/tie tokens\n");
    lxx::dsp::StepSequencer seq;
    lxx::dsp::Sequence sequence;
    sequence.numNotes = 4;
    sequence.notes = { 60, 0xff, 0xfe, 62 };
    seq.setSequence(sequence);
    seq.start();

    const auto e0 = seq.clockStep();
    const auto e1 = seq.clockStep();
    const auto e2 = seq.clockStep();
    const auto e3 = seq.clockStep();
    const auto e4 = seq.clockStep();

    expectTrue(e0.size() == 1 && e0[0].type == lxx::dsp::SequenceEventType::note, "step0 note");
    expectTrue(e1.size() == 1 && e1[0].type == lxx::dsp::SequenceEventType::rest, "step1 rest");
    expectTrue(e2.size() == 1 && e2[0].type == lxx::dsp::SequenceEventType::tie, "step2 tie");
    expectTrue(e3.size() == 1 && e3[0].note == 62, "step3 note value");
    expectTrue(e4.size() == 1 && e4[0].note == 60, "wrapped to step0");
}

void testDrumPatternOverride()
{
    std::printf("Test: drums_pattern override bitmask\n");
    lxx::dsp::DrumSequencer drums;
    lxx::dsp::SequencerSettings settings;
    settings.drumsOverride = 0x01;  // BD override
    settings.drumsPattern[0] = 0b0000000000000101;  // steps 0 and 2
    drums.start();

    const auto s0 = drums.clockStep(settings, true);
    const auto s1 = drums.clockStep(settings, true);
    const auto s2 = drums.clockStep(settings, true);

    expectTrue(!s0.empty() && s0[0].part == 0, "BD hits on step 0");
    expectTrue(s1.empty(), "no BD hit on step 1");
    expectTrue(!s2.empty() && s2[0].part == 0, "BD hits on step 2");
}

void testGenerativeDrumMap()
{
    std::printf("Test: generative ReadDrumMap path (no override)\n");
    lxx::dsp::DrumSequencer drums;
    lxx::dsp::SequencerSettings settings;
    settings.drumsX = 128;
    settings.drumsY = 128;
    settings.drumsDensity = { 200, 200, 200 };
    settings.drumsOverride = 0;
    uint32_t rng = 0x1234567u;
    drums.start();

    int hits = 0;
    for (int step = 0; step < 32; ++step) {
        const auto events = drums.clockStep(settings, true, &rng);
        hits += static_cast<int>(events.size());
    }
    expectTrue(hits > 4, "generative map produces multiple hits over 2 bars");
}

void testHostRateClockEquivalence()
{
    std::printf("Test: 48 kHz sample path matches firmware tick stepping\n");
    lxx::dsp::ClockEngine fwClock;
    lxx::dsp::ClockEngine hostClock;
    fwClock.reset();
    hostClock.reset();
    fwClock.update(100, 0, 1, 1);
    hostClock.update(100, 0, 1, 1);

    constexpr int kTicks = 200000;
    int fwSteps = 0;
    for (int tick = 0; tick < kTicks; ++tick) {
        fwClock.tickFirmwareTimer();
        fwClock.countEvents();
        while (fwClock.consumeStepPulse()) {
            ++fwSteps;
        }
    }

    constexpr double kSampleRate = 48000.0;
    const uint32_t block = 64;
    const uint32_t targetSamples = static_cast<uint32_t>(kTicks * (kSampleRate / lxx::dsp::kFirmwareClockHz));
    int hostSteps = 0;
    uint32_t samples = 0;
    while (samples < targetSamples) {
        hostClock.processSamples(block, kSampleRate);
        samples += block;
        while (hostClock.consumeStepPulse()) {
            ++hostSteps;
        }
    }

    const std::string msg = "firmware-tick and host-sample clocks emit same step pulses (fw="
                            + std::to_string(fwSteps) + " host=" + std::to_string(hostSteps) + ")";
    expectTrue(fwSteps == hostSteps, msg.c_str());
}

}  // namespace

int main()
{
    testIndependentStepTiming();
    testSwingAlternation();
    testArpPatternMaskCycle();
    testSequenceWrapAndSpecialNotes();
    testDrumPatternOverride();
    testGenerativeDrumMap();
    testHostRateClockEquivalence();

    if (failures == 0) {
        std::printf("PASS: all sequencer/clock validation checks (%d)\n", 6);
        return 0;
    }

    std::printf("FAILED: %d validation checks\n", failures);
    return 1;
}
