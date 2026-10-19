#pragma once

#include <vector>

#include "sequencer/sequencer_types.h"

namespace lxx {
namespace dsp {

class StepSequencer {
public:
    void start();
    void stop();

    void setSequence(const Sequence& sequence) { sequence_ = sequence; }
    void setTransposition(int8_t semitones) { transposition_ = semitones; }
    void setVelocity(uint8_t velocity) { velocity_ = velocity; }

    bool running() const { return running_; }
    uint8_t currentStep() const { return stepIndex_; }

    std::vector<SeqNoteEvent> clockStep();

private:
    Sequence sequence_ {};
    bool running_ = false;
    uint8_t stepIndex_ = 0;
    int8_t transposition_ = 0;
    uint8_t velocity_ = 64;
};

}  // namespace dsp
}  // namespace lxx
