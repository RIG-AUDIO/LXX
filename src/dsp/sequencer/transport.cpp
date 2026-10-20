#include "sequencer/transport.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace lxx {
namespace dsp {

void SequencerTransport::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    touchClock();
}

void SequencerTransport::reset()
{
    running_ = false;
    recording_ = false;
    pressedKeys_.clear();
    clock_.reset();
    wasHostPlaying_ = false;
    hostAligned_ = false;
    lastHostStep_ = 0;
    expectedPpq_ = 0.0;
    touchClock();
}

void SequencerTransport::setSettings(const SequencerSettings& settings)
{
    settings_ = settings;
    touchClock();
    arpeggiator_.setSettings(settings_);
}

void SequencerTransport::setSequence(const Sequence& sequence)
{
    sequence_ = sequence;
    stepSequencer_.setSequence(sequence_);
}

void SequencerTransport::touchClock()
{
    clock_.update(settings_.tempo, settings_.swing, ppqnIndex_, 1);
}

void SequencerTransport::start()
{
    if (!(pressedKeys_.size() > 0 && sequence_.numNotes == 0)) {
        pressedKeys_.clear();
    }
    if (recording_) {
        stopRecording();
    }
    stepSequencer_.start();
    arpeggiator_.start();
    drumSequencer_.start();
    clock_.reset();
    running_ = true;
}

void SequencerTransport::stop()
{
    running_ = false;
    stepSequencer_.stop();
    arpeggiator_.stop();
}

void SequencerTransport::startRecording()
{
    stop();
    settings_.arpMode = 0;
    recording_ = true;
    sequence_ = {};
    stepSequencer_.setSequence(sequence_);
}

void SequencerTransport::stopRecording()
{
    stop();
    recording_ = false;
    stepSequencer_.setTransposition(0);
    stepSequencer_.setVelocity(64);
}

void SequencerTransport::insertRest()
{
    if (!recording_ || sequence_.numNotes >= kMaxSequenceSteps) {
        return;
    }
    sequence_.notes[sequence_.numNotes++] = 0xff;
    if (sequence_.numNotes >= kMaxSequenceSteps) {
        stopRecording();
    }
    stepSequencer_.setSequence(sequence_);
}

void SequencerTransport::insertTie()
{
    if (!recording_ || sequence_.numNotes >= kMaxSequenceSteps) {
        return;
    }
    sequence_.notes[sequence_.numNotes++] = 0xfe;
    if (sequence_.numNotes >= kMaxSequenceSteps) {
        stopRecording();
    }
    stepSequencer_.setSequence(sequence_);
}

void SequencerTransport::noteOn(uint8_t note, uint8_t velocity)
{
    if (velocity == 0) {
        noteOff(note);
        return;
    }

    if (running_ && sequence_.numNotes > 0) {
        stepSequencer_.setTransposition(static_cast<int8_t>(note - 60));
        stepSequencer_.setVelocity(velocity);
    } else {
        pressedKeys_.noteOn(note, velocity);
        if (settings_.arpMode == 0) {
            // Voice trigger handled by host.
        } else if (pressedKeys_.size() == 1 && !running_) {
            start();
        }
    }

    if (recording_ && sequence_.numNotes < kMaxSequenceSteps) {
        sequence_.notes[sequence_.numNotes++] = note;
        if (sequence_.numNotes >= kMaxSequenceSteps) {
            stopRecording();
        }
        stepSequencer_.setSequence(sequence_);
    }
}

void SequencerTransport::noteOff(uint8_t note)
{
    if (running_ && sequence_.numNotes > 0) {
        return;
    }

    if (settings_.arpMode == 0) {
        pressedKeys_.noteOff(note);
    } else {
        pressedKeys_.noteOff(note);
        if (pressedKeys_.size() == 0 && !running_) {
            stop();
        }
    }
}

void SequencerTransport::allNotesOff()
{
    pressedKeys_.clear();
    arpeggiator_.setPressedKeys(pressedKeys_);
}

void SequencerTransport::midiClockTick()
{
    if (!usesInternalClock()) {
        clock_.tickFirmwareTimer();
        clock_.countEvents();
    }
}

void SequencerTransport::analogClockTick()
{
    if (!usesInternalClock()) {
        clock_.tickFirmwareTimer();
        clock_.countEvents();
    }
}

TransportEvents SequencerTransport::dispatchStep(uint8_t)
{
    TransportEvents events;
    events.stepPulse = true;

    if (running_) {
        const auto drumEvents = drumSequencer_.clockStep(settings_, running_, &randomState_);
        events.drums.append(drumEvents);
    }

    return events;
}

TransportEvents SequencerTransport::processBlock(uint32_t numSamples)
{
    return processBlock(numSamples, HostClockInput {});
}

TransportEvents SequencerTransport::processBlock(uint32_t numSamples, const HostClockInput& hostClock)
{
    if (hostClock.enabled)
        return processHostClock(numSamples, hostClock);

    wasHostPlaying_ = false;
    hostAligned_ = false;
    return processInternalClock(numSamples);
}

TransportEvents SequencerTransport::processInternalClock(uint32_t numSamples)
{
    TransportEvents events;
    if (!running_ && !recording_) {
        return events;
    }

    clock_.processSamples(numSamples, sampleRate_);
    int pulses = 0;
    while (clock_.consumeStepPulse() && pulses < 16) {
        ++pulses;
        randomState_ = randomState_ * 1664525u + 1013904223u;
        const auto stepEvents = dispatchStep(static_cast<uint8_t>((randomState_ >> 24) & 0xff));
        events.arp.append(stepEvents.arp);
        events.sequence.append(stepEvents.sequence);
        events.drums.append(stepEvents.drums);
        events.stepPulse = events.stepPulse || stepEvents.stepPulse;
    }
    return events;
}

TransportEvents SequencerTransport::processHostClock(uint32_t numSamples, const HostClockInput& host)
{
    TransportEvents events;
    if (!host.isPlaying) {
        if (wasHostPlaying_) {
            events.releaseHeldArp = true;
            arpeggiator_.stop();
        }
        wasHostPlaying_ = false;
        hostAligned_ = false;
        return events;
    }

    if (!running_ && !recording_) {
        wasHostPlaying_ = true;
        hostAligned_ = false;
        return events;
    }

    const double sr = host.sampleRate > 1.0 ? host.sampleRate : sampleRate_;
    const double bpm = host.bpm > 1.0 ? host.bpm : 120.0;
    const double ppqPerSample = bpm / (60.0 * sr);
    const double blockPpq = ppqPerSample * static_cast<double>(numSamples);
    const double ppqPerStep = ppqPerClockStep(host.divisionIndex);

    if (!host.hasPpq) {
        if (!wasHostPlaying_ || !hostAligned_) {
            hostAligned_ = true;
            wasHostPlaying_ = true;
            clock_.reset();
        }
        const uint8_t bpm8 = static_cast<uint8_t>(std::clamp(std::lround(bpm), 40L, 240L));
        clock_.update(bpm8, settings_.swing, ppqnIndex_, 1);
        return processInternalClock(numSamples);
    }

    const double ppq = host.ppq;
    const int64_t step = static_cast<int64_t>(std::floor(ppq / ppqPerStep));
    const bool discontinuity = wasHostPlaying_ && hostAligned_
        && std::abs(ppq - expectedPpq_) > (blockPpq * 2.0 + ppqPerStep);

    if (!wasHostPlaying_ || !hostAligned_ || discontinuity) {
        lastHostStep_ = step;
        hostAligned_ = true;
        wasHostPlaying_ = true;
        expectedPpq_ = ppq + blockPpq;
        if (!discontinuity) {
            randomState_ = randomState_ * 1664525u + 1013904223u;
            return dispatchStep(static_cast<uint8_t>((randomState_ >> 24) & 0xff));
        }
        return events;
    }

    int64_t delta = step - lastHostStep_;
    if (delta < 0)
        delta = 1;
    if (delta > 16)
        delta = 1;
    for (int64_t i = 0; i < delta; ++i) {
        randomState_ = randomState_ * 1664525u + 1013904223u;
        const auto stepEvents = dispatchStep(static_cast<uint8_t>((randomState_ >> 24) & 0xff));
        events.arp.append(stepEvents.arp);
        events.sequence.append(stepEvents.sequence);
        events.drums.append(stepEvents.drums);
        events.stepPulse = true;
    }
    lastHostStep_ = step;
    expectedPpq_ = ppq + blockPpq;
    wasHostPlaying_ = true;
    return events;
}

}  // namespace dsp
}  // namespace lxx
