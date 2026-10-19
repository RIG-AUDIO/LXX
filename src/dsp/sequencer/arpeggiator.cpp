#include "sequencer/arpeggiator.h"

namespace lxx {
namespace dsp {

namespace {
uint8_t u8u8Mul(uint8_t a, uint8_t b)
{
    return static_cast<uint8_t>((static_cast<uint16_t>(a) * b) >> 8);
}

uint8_t u8u8MulShift8(uint8_t a, uint8_t b)
{
    return static_cast<uint8_t>((static_cast<uint16_t>(a) * b) >> 8);
}
}  // namespace

void Arpeggiator::resetPattern()
{
    if (direction_ == 1) {
        octave_ = 0;
        step_ = 0;
    } else {
        step_ = static_cast<int8_t>(pressedKeys_.size() - 1);
        octave_ = static_cast<int8_t>(settings_.arpRange() - 1);
    }
}

void Arpeggiator::start()
{
    patternMask_ = 1;
    patternStep_ = 0;
    direction_ = settings_.arpDirection() == static_cast<uint8_t>(ArpeggiatorDirection::down) ? -1 : 1;
    resetPattern();
}

void Arpeggiator::stop()
{
    patternMask_ = 1;
    patternStep_ = 0;
}

std::vector<ArpNoteEvent> Arpeggiator::clockStep(uint8_t randomByte)
{
    std::vector<ArpNoteEvent> events;
    if (!hasArpeggiator()) {
        ++patternStep_;
        patternMask_ = static_cast<uint16_t>(patternMask_ << 1);
        if (patternMask_ == 0) {
            patternMask_ = 1;
            patternStep_ = 0;
        }
        return events;
    }

    const uint16_t pattern = kArpeggiatorPatterns[settings_.arpPattern % kArpeggiatorPatterns.size()];
    const bool hasNote = (patternMask_ & pattern) != 0;

    if (hasNote) {
        stepArpeggiator(randomByte);
        const NoteEntry& arpNote = pressedKeys_.sortedNote(static_cast<uint8_t>(step_));
        uint8_t note = arpNote.note;
        const uint8_t velocity = arpNote.velocity;
        note = static_cast<uint8_t>(note + 12 * octave_);
        while (note > 127) {
            note = static_cast<uint8_t>(note - 12);
            if (note > 240)
                break;
        }

        uint8_t slideThreshold = u8u8Mul(kArpSlideProbability[patternStep_], settings_.acidity);
        if (step_ == 0) {
            slideThreshold >>= 1;
        }

        uint8_t accent = u8u8Mul(kArpAccentLevel[patternStep_], settings_.acidity) >> 1;
        accent = static_cast<uint8_t>((accent + u8u8MulShift8(accent, randomByte)) >> 1);

        ArpNoteEvent event;
        event.note = note;
        event.velocity = velocity;
        event.accent = accent;
        event.slide = randomByte < slideThreshold;
        event.gate = true;
        events.push_back(event);
    }

    ++patternStep_;
    patternMask_ = static_cast<uint16_t>(patternMask_ << 1);
    if (patternMask_ == 0) {
        patternMask_ = 1;
        patternStep_ = 0;
    }

    return events;
}

void Arpeggiator::stepArpeggiator(uint8_t randomByte)
{
    const uint8_t numNotes = pressedKeys_.size();
    if (settings_.arpDirection() == static_cast<uint8_t>(ArpeggiatorDirection::random)) {
        octave_ = static_cast<int8_t>(randomByte & 0x0f);
        step_ = static_cast<int8_t>((randomByte & 0xf0) >> 4);
        if (settings_.arpRange() == 0) {
            octave_ = 0;
        } else {
            int guard = 0;
            while (octave_ >= static_cast<int8_t>(settings_.arpRange()) && guard++ < 16) {
                octave_ = static_cast<int8_t>(octave_ - settings_.arpRange());
            }
        }
        if (numNotes == 0) {
            step_ = 0;
            return;
        }
        int guard = 0;
        while (step_ >= static_cast<int8_t>(numNotes) && guard++ < 16) {
            step_ = static_cast<int8_t>(step_ - numNotes);
        }
        return;
    }

    step_ = static_cast<int8_t>(step_ + direction_);
    uint8_t changeOctave = 0;
    if (step_ >= static_cast<int8_t>(numNotes)) {
        step_ = 0;
        changeOctave = 1;
    } else if (step_ < 0) {
        step_ = static_cast<int8_t>(numNotes - 1);
        changeOctave = 1;
    }

    if (changeOctave != 0) {
        octave_ = static_cast<int8_t>(octave_ + direction_);
        if (octave_ >= static_cast<int8_t>(settings_.arpRange()) || octave_ < 0) {
            if (settings_.arpDirection() == static_cast<uint8_t>(ArpeggiatorDirection::upDown)) {
                direction_ = static_cast<int8_t>(-direction_);
                resetPattern();
                if (numNotes > 1 || settings_.arpRange() > 1) {
                    stepArpeggiator(randomByte);
                }
            } else {
                resetPattern();
            }
        }
    }
}

}  // namespace dsp
}  // namespace lxx
