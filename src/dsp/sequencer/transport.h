#pragma once

#include <cstdint>

#include "sequencer/arpeggiator.h"
#include "sequencer/clock.h"
#include "sequencer/drum_sequencer.h"
#include "voice/note_stack.h"
#include "sequencer/sequencer_types.h"
#include "sequencer/step_sequencer.h"

namespace lxx {
namespace dsp {

// The transport runs on the audio thread, so the event lists are fixed
// capacity: no heap allocation happens while rendering.  Capacities follow
// the structural bounds of the transport (at most kMaxStepPulsesPerBlock
// step pulses per block, at most kNumDrumParts drum parts per step).
struct TransportEvents {
    FixedEventList<ArpNoteEvent, kMaxArpEventsPerBlock> arp;
    FixedEventList<SeqNoteEvent, kMaxSequenceEventsPerBlock> sequence;
    FixedEventList<DrumHitEvent, kMaxTransportDrumEvents> drums;
    bool stepPulse = false;
    bool releaseHeldArp = false;
};

class SequencerTransport {
public:
    void prepare(double sampleRate);
    void reset();

    void setSettings(const SequencerSettings& settings);
    void setSequence(const Sequence& sequence);
    void setPpqnIndex(uint8_t ppqnIndex) { ppqnIndex_ = ppqnIndex > 2 ? 2 : ppqnIndex; }

    void start();
    void stop();
    bool running() const { return running_; }

    void startRecording();
    void stopRecording();
    bool recording() const { return recording_; }

    void insertRest();
    void insertTie();

    void noteOn(uint8_t note, uint8_t velocity);
    void noteOff(uint8_t note);
    void allNotesOff();

    void midiClockTick();
    void analogClockTick();

    TransportEvents processBlock(uint32_t numSamples);
    TransportEvents processBlock(uint32_t numSamples, const HostClockInput& hostClock);

    const NoteStack<kNoteStackCapacity>& pressedKeys() const { return pressedKeys_; }
    const Sequence& sequence() const { return sequence_; }
    Sequence& mutableSequence() { return sequence_; }
    uint8_t sequencerStep() const { return stepSequencer_.currentStep(); }
    uint8_t drumStep() const { return drumSequencer_.currentStep(); }
    uint8_t grooveStep() const { return clock_.grooveStep(); }
    bool usesInternalClock() const { return !settings_.hostSync; }

    ClockEngine& clock() { return clock_; }
    const ClockEngine& clock() const { return clock_; }
    Arpeggiator& arpeggiator() { return arpeggiator_; }

private:
    void touchClock();
    TransportEvents dispatchStep(uint8_t randomByte);
    TransportEvents processInternalClock(uint32_t numSamples);
    TransportEvents processHostClock(uint32_t numSamples, const HostClockInput& host);

    double sampleRate_ = 48000.0;
    uint8_t ppqnIndex_ = 0;
    bool running_ = false;
    bool recording_ = false;
    uint32_t randomState_ = 0x1234567u;
    bool wasHostPlaying_ = false;
    bool hostAligned_ = false;
    int64_t lastHostStep_ = 0;
    double expectedPpq_ = 0.0;

    SequencerSettings settings_ {};
    Sequence sequence_ {};
    NoteStack<kNoteStackCapacity> pressedKeys_ {};

    ClockEngine clock_;
    Arpeggiator arpeggiator_;
    StepSequencer stepSequencer_;
    DrumSequencer drumSequencer_;
};

}  // namespace dsp
}  // namespace lxx
