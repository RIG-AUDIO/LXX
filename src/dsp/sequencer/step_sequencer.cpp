#include "sequencer/step_sequencer.h"

namespace lxx {
namespace dsp {

void StepSequencer::start()
{
    stepIndex_ = 0;
    running_ = true;
}

void StepSequencer::stop()
{
    running_ = false;
}

std::vector<SeqNoteEvent> StepSequencer::clockStep()
{
    std::vector<SeqNoteEvent> events;
    if (!running_ || sequence_.numNotes == 0) {
        return events;
    }

    const uint8_t raw = sequence_.notes[stepIndex_];
    SeqNoteEvent event;
    if (raw == 0xff) {
        event.type = SequenceEventType::rest;
        event.gate = false;
    } else if (raw == 0xfe) {
        event.type = SequenceEventType::tie;
        event.gate = false;
    } else {
        const uint8_t accentSlideIndex = stepIndex_ >> 3;
        const uint8_t accentSlideMask = static_cast<uint8_t>(1u << (stepIndex_ & 0x7u));
        event.type = SequenceEventType::note;
        event.note = static_cast<uint8_t>(raw + transposition_);
        event.velocity = velocity_;
        event.slide = (sequence_.slides[accentSlideIndex] & accentSlideMask) != 0;
        event.accent = (sequence_.accents[accentSlideIndex] & accentSlideMask) != 0;
        event.gate = true;
    }

    events.push_back(event);

    ++stepIndex_;
    if (stepIndex_ >= sequence_.numNotes) {
        stepIndex_ = 0;
    }

    return events;
}

}  // namespace dsp
}  // namespace lxx
