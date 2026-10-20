#pragma once

#include <cstdint>

#include "voice/note_stack.h"
#include "sequencer/sequencer_resources.h"
#include "sequencer/sequencer_types.h"

namespace lxx {
namespace dsp {

class Arpeggiator {
public:
    void resetPattern();
    void start();
    void stop();

    void setSettings(const SequencerSettings& settings) { settings_ = settings; }
    void setPressedKeys(const NoteStack<kNoteStackCapacity>& keys) { pressedKeys_ = keys; }

    bool hasArpeggiator() const { return settings_.arpMode != 0 && pressedKeys_.size() > 0; }

    // Called once per transport step pulse.
    FixedEventList<ArpNoteEvent, kMaxArpEventsPerStep> clockStep(uint8_t randomByte);

    uint8_t patternStep() const { return patternStep_; }
    uint16_t patternMask() const { return patternMask_; }

private:
    void stepArpeggiator(uint8_t randomByte);

    SequencerSettings settings_ {};
    NoteStack<kNoteStackCapacity> pressedKeys_ {};

    uint16_t patternMask_ = 1;
    uint8_t patternStep_ = 0;
    int8_t direction_ = 1;
    int8_t step_ = 0;
    int8_t octave_ = 0;
};

}  // namespace dsp
}  // namespace lxx
