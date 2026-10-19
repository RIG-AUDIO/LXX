#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace lxx {
namespace dsp {

static constexpr uint8_t kNumDrumParts = 3;
static constexpr uint8_t kMaxSequenceSteps = 128;
static constexpr uint8_t kDrumSteps = 16;
static constexpr uint8_t kGrooveSteps = 16;
static constexpr uint8_t kTicksPerGrooveStep = 6;
static constexpr uint8_t kNoteStackCapacity = 16;

// Firmware timer ISR rate (anu.cc TIMER2_OVF_vect comment: 39 kHz).
static constexpr double kFirmwareClockHz = 39000.0;

enum class ArpeggiatorDirection : uint8_t {
    up = 0,
    down,
    upDown,
    random,
    last
};

enum class SequenceEventType : uint8_t {
    note,
    rest,  // 0xff
    tie    // 0xfe
};

struct HostClockInput {
    bool enabled = false;
    bool isPlaying = false;
    bool hasPpq = false;
    double bpm = 120.0;
    double ppq = 0.0;
    double sampleRate = 48000.0;
    uint8_t divisionIndex = 4;
};

inline double ppqPerClockStep(uint8_t divisionIndex)
{
    static constexpr double kTable[] = { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125 };
    const unsigned i = divisionIndex > 5 ? 4u : static_cast<unsigned>(divisionIndex);
    return kTable[i];
}

inline double syncedLfoHz(double bpm, uint8_t divisionIndex)
{
    const double beats = ppqPerClockStep(divisionIndex);
    if (!(bpm > 1.0) || !(beats > 1.0e-6))
        return 2.0;
    return bpm / (60.0 * beats);
}

struct SequencerSettings {
    uint8_t arpMode = 0;
    uint8_t arpPattern = 0;
    uint8_t tempo = 120;
    uint8_t swing = 0;
    bool hostSync = true;
    uint8_t clockDivision = 4;

    uint8_t drumsX = 0;
    uint8_t drumsY = 0;
    std::array<uint8_t, kNumDrumParts> drumsDensity {};
    std::array<uint8_t, kNumDrumParts> drumsTone {};
    uint8_t drumsBalance = 128;
    uint8_t drumsBandwidth = 255;

    uint8_t drumsOverride = 0;
    std::array<uint16_t, kNumDrumParts> drumsPattern {};

    uint8_t acidity = 0;

    uint8_t arpRange() const { return static_cast<uint8_t>(((arpMode - 1) & 0x01u) + 1u); }
    uint8_t arpDirection() const { return static_cast<uint8_t>((arpMode - 1) >> 1); }
    bool hasDrums() const {
        return drumsDensity[0] > 1 || drumsDensity[1] > 1 || drumsDensity[2] > 1 || drumsOverride != 0;
    }
};

struct Sequence {
    uint8_t numNotes = 0;
    std::array<uint8_t, kMaxSequenceSteps> notes {};
    std::array<uint8_t, 16> accents {};
    std::array<uint8_t, 16> slides {};
};

struct ArpNoteEvent {
    uint8_t note = 0;
    uint8_t velocity = 0;
    uint8_t accent = 0;
    bool slide = false;
    bool gate = false;
};

struct SeqNoteEvent {
    uint8_t note = 0;
    uint8_t velocity = 0;
    bool accent = false;
    bool slide = false;
    bool gate = false;
    SequenceEventType type = SequenceEventType::note;
};

struct DrumHitEvent {
    uint8_t part = 0;
    uint8_t level = 0;
};

// Allocation-free event list.
//
// The sequencer runs on the audio thread, so the event lists it hands around
// must never touch the heap.  Capacities come from the structural bounds of
// the transport: at most 16 step pulses per audio block (see
// SequencerTransport::processInternalClock) and at most kNumDrumParts drum
// parts per step.
template <typename T, std::size_t Capacity>
class FixedEventList {
public:
    bool empty() const noexcept { return count_ == 0; }
    std::size_t size() const noexcept { return count_; }
    static constexpr std::size_t capacity() noexcept { return Capacity; }

    const T& operator[](std::size_t i) const noexcept { return items_[i]; }
    T& operator[](std::size_t i) noexcept { return items_[i]; }

    void clear() noexcept { count_ = 0; }

    bool push(const T& event) noexcept
    {
        if (count_ >= Capacity)
            return false;
        items_[count_++] = event;
        return true;
    }

    // Appends the contents of another list; events beyond the capacity are
    // dropped, which cannot happen within the structural bounds above.
    void append(const FixedEventList& other) noexcept
    {
        for (std::size_t i = 0; i < other.count_ && count_ < Capacity; ++i)
            items_[count_++] = other.items_[i];
    }

    const T* begin() const noexcept { return items_.data(); }
    const T* end() const noexcept { return items_.data() + count_; }
    T* begin() noexcept { return items_.data(); }
    T* end() noexcept { return items_.data() + count_; }

private:
    std::array<T, Capacity> items {};
    std::size_t count_ = 0;
};

// Structural bounds of the transport, used as list capacities.
static constexpr std::size_t kMaxStepPulsesPerBlock = 16;
static constexpr std::size_t kMaxArpEventsPerBlock = kMaxStepPulsesPerBlock;
static constexpr std::size_t kMaxSequenceEventsPerBlock = kMaxStepPulsesPerBlock;
// Structural bound is kMaxStepPulsesPerBlock * kNumDrumParts == 48; the extra
// headroom keeps the list safe if a host ever delivers an extra pulse, and it
// stays below the 96-event engine buffer (kMaxDrumEventsPerBlock).
static constexpr std::size_t kMaxDrumEventsPerBlock = 64;
static constexpr std::size_t kMaxArpEventsPerStep = 4;
static constexpr std::size_t kMaxSequenceEventsPerStep = 1;

}  // namespace dsp
}  // namespace lxx
