#pragma once

#include <cstdint>
#include <cstring>

namespace lxx {
namespace dsp {

/**
 * Monophonic note stack — port of upstream anu/note_stack.h (LIFO priority).
 *
 * Most recently pressed note is played. On note-off of the active note,
 * the previous held note is retriggered with legato=true (voice_controller.cc).
 *
 * Firmware walks are unbounded and treat free slots (nextPtr==0) as the
 * eviction tail. A cycle or a missed free-slot search can hang the audio
 * thread or increment size_ past capacity (OOB on sortedPtr_). Every walk
 * here is bounded by kCapacity; size_ never exceeds kCapacity.
 */
struct NoteEntry {
    uint8_t note = 0xff;
    uint8_t velocity = 0;
    uint8_t nextPtr = 0;
};

template <uint8_t kCapacity = 16>
class NoteStack {
public:
    static constexpr uint8_t kFreeSlot = 0xff;

    NoteStack() { clear(); }
    void init() { clear(); }

    void noteOn(uint8_t note, uint8_t velocity) {
        noteOff(note);

        if (size_ >= kCapacity) {
            const uint8_t leastRecent = leastRecentOccupiedNote();
            if (leastRecent != kFreeSlot)
                noteOff(leastRecent);
        }
        if (size_ >= kCapacity)
            return;

        const uint8_t freeSlot = findFreeSlot();
        if (freeSlot == 0)
            return;

        pool_[freeSlot].nextPtr = rootPtr_;
        pool_[freeSlot].note = note;
        pool_[freeSlot].velocity = velocity;
        rootPtr_ = freeSlot;

        uint8_t inserted = 0;
        const uint8_t live = size_;
        for (uint8_t i = 0; i < live && i < kCapacity; ++i) {
            const uint8_t sortedIdx = sortedPtr_[i];
            if (sortedIdx == 0 || sortedIdx > kCapacity)
                continue;
            if (pool_[sortedIdx].note > note) {
                for (uint8_t j = live; j > i; --j) {
                    if (j <= kCapacity)
                        sortedPtr_[j] = sortedPtr_[j - 1];
                }
                sortedPtr_[i] = freeSlot;
                inserted = 1;
                break;
            }
        }
        if (!inserted && live < kCapacity) {
            sortedPtr_[live] = freeSlot;
        }
        if (size_ < kCapacity)
            ++size_;
    }

    void noteOff(uint8_t note) {
        uint8_t current = rootPtr_;
        uint8_t previous = 0;
        uint8_t steps = 0;
        while (current != 0 && current <= kCapacity && steps < kCapacity) {
            if (pool_[current].note == note)
                break;
            previous = current;
            const uint8_t next = pool_[current].nextPtr;
            if (next == current)
                break;
            current = next;
            ++steps;
        }
        if (current == 0 || current > kCapacity || pool_[current].note != note)
            return;

        if (previous != 0 && previous <= kCapacity) {
            pool_[previous].nextPtr = pool_[current].nextPtr;
        } else {
            rootPtr_ = pool_[current].nextPtr;
            if (rootPtr_ > kCapacity)
                rootPtr_ = 0;
        }

        const uint8_t live = size_ > kCapacity ? kCapacity : size_;
        for (uint8_t i = 0; i < live; ++i) {
            if (sortedPtr_[i] == current) {
                for (uint8_t j = i; j + 1 < live; ++j)
                    sortedPtr_[j] = sortedPtr_[j + 1];
                sortedPtr_[live > 0 ? live - 1 : 0] = 0;
                break;
            }
        }

        pool_[current].nextPtr = 0;
        pool_[current].note = kFreeSlot;
        pool_[current].velocity = 0;
        if (size_ > 0)
            --size_;
    }

    void clear() {
        size_ = 0;
        for (auto& entry : pool_)
            entry = NoteEntry{};
        std::memset(sortedPtr_, 0, sizeof(sortedPtr_));
        rootPtr_ = 0;
        for (uint8_t i = 0; i <= kCapacity; ++i)
            pool_[i].note = kFreeSlot;
    }

    uint8_t size() const { return size_ > kCapacity ? kCapacity : size_; }
    bool empty() const { return size() == 0 || rootPtr_ == 0; }

    const NoteEntry& mostRecent() const {
        if (rootPtr_ == 0 || rootPtr_ > kCapacity)
            return pool_[0];
        return pool_[rootPtr_];
    }
    const NoteEntry& sortedNote(uint8_t index) const {
        if (index >= size())
            return pool_[0];
        const uint8_t slot = sortedPtr_[index];
        if (slot == 0 || slot > kCapacity)
            return pool_[0];
        return pool_[slot];
    }

    uint8_t mostRecentNote() const { return mostRecent().note; }
    uint8_t mostRecentVelocity() const { return mostRecent().velocity; }

private:
    uint8_t findFreeSlot() const {
        for (uint8_t i = 1; i <= kCapacity; ++i) {
            if (pool_[i].note == kFreeSlot)
                return i;
        }
        return 0;
    }

    uint8_t leastRecentOccupiedNote() const {
        uint8_t current = rootPtr_;
        uint8_t lastNote = kFreeSlot;
        uint8_t steps = 0;
        while (current != 0 && current <= kCapacity && steps < kCapacity) {
            if (pool_[current].note != kFreeSlot)
                lastNote = pool_[current].note;
            const uint8_t next = pool_[current].nextPtr;
            if (next == 0 || next == current)
                break;
            current = next;
            ++steps;
        }
        return lastNote;
    }

    uint8_t size_ = 0;
    NoteEntry pool_[kCapacity + 1]{};
    uint8_t rootPtr_ = 0;
    uint8_t sortedPtr_[kCapacity + 1]{};
};

} // namespace dsp
} // namespace lxx
