#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace lxx {
namespace dsp {

// Processor audio is rendered in bounded host chunks so MIDI sample offsets can
// be remapped. Voice already updates filter coefficients on 32-sample subblocks.
inline constexpr size_t kHostProcessChunk = 32;

// JUCE MidiBuffer: samplePosition is relative to the host block, typically in
// the half-open interval [0, numSamples). An event belongs to exactly one chunk.
inline bool midiEventInChunk(int hostOffset, size_t chunkStart, size_t chunkSize) noexcept
{
    if (chunkSize == 0)
        return false;
    if (hostOffset < 0)
        return chunkStart == 0;
    const auto off = static_cast<size_t>(hostOffset);
    return off >= chunkStart && off < chunkStart + chunkSize;
}

inline int midiLocalOffset(int hostOffset, size_t chunkStart) noexcept
{
    if (hostOffset < 0)
        return 0;
    const int local = hostOffset - static_cast<int>(chunkStart);
    return local < 0 ? 0 : local;
}

inline size_t nextHostChunkSize(size_t remaining, size_t capacity) noexcept
{
    const size_t limited = std::min(remaining, capacity);
    const size_t chunk = std::min(limited, kHostProcessChunk);
    return chunk;
}

} // namespace dsp
} // namespace lxx
