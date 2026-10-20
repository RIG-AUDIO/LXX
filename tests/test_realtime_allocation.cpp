// Realtime-safety evidence: the audio path must not allocate.
//
// The sequencer transport used to build its event lists with std::vector,
// so every step pulse allocated on the audio thread.  The event lists are
// now fixed-capacity (FixedEventList), and this test proves the property
// instead of asserting it in prose: global operator new / delete are
// replaced with counting versions, the code path is warmed up, and the
// counter must stay at zero while the transport renders.
//
// The replacement is local to this test binary; the plugin is unaffected.
#include "../src/dsp/sequencer/transport.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <new>

namespace {

std::atomic<bool> g_armed { false };
std::atomic<unsigned long> g_allocations { 0 };
std::atomic<unsigned long> g_frees { 0 };

bool armed() noexcept
{
    return g_armed.load(std::memory_order_relaxed);
}

} // namespace

// --- global replacements ---------------------------------------------------
void* operator new(std::size_t size)
{
    if (armed())
        g_allocations.fetch_add(1, std::memory_order_relaxed);
    void* p = std::malloc(size != 0 ? size : 1);
    if (p == nullptr)
        throw std::bad_alloc();
    return p;
}

void* operator new[](std::size_t size)
{
    if (armed())
        g_allocations.fetch_add(1, std::memory_order_relaxed);
    void* p = std::malloc(size != 0 ? size : 1);
    if (p == nullptr)
        throw std::bad_alloc();
    return p;
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
    if (armed())
        g_allocations.fetch_add(1, std::memory_order_relaxed);
    return std::malloc(size != 0 ? size : 1);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
    if (armed())
        g_allocations.fetch_add(1, std::memory_order_relaxed);
    return std::malloc(size != 0 ? size : 1);
}

void* operator new(std::size_t size, std::align_val_t alignment)
{
    if (armed())
        g_allocations.fetch_add(1, std::memory_order_relaxed);
    const std::size_t align = static_cast<std::size_t>(alignment);
    const std::size_t rounded = ((size != 0 ? size : 1) + align - 1) / align * align;
    void* p = std::aligned_alloc(align, rounded);
    if (p == nullptr)
        throw std::bad_alloc();
    return p;
}

void* operator new[](std::size_t size, std::align_val_t alignment)
{
    return ::operator new(size, alignment);
}

void operator delete(void* p) noexcept
{
    if (armed())
        g_frees.fetch_add(1, std::memory_order_relaxed);
    std::free(p);
}

void operator delete[](void* p) noexcept
{
    if (armed())
        g_frees.fetch_add(1, std::memory_order_relaxed);
    std::free(p);
}

void operator delete(void* p, std::size_t) noexcept
{
    if (armed())
        g_frees.fetch_add(1, std::memory_order_relaxed);
    std::free(p);
}

void operator delete[](void* p, std::size_t) noexcept
{
    if (armed())
        g_frees.fetch_add(1, std::memory_order_relaxed);
    std::free(p);
}

void operator delete(void* p, const std::nothrow_t&) noexcept
{
    if (armed())
        g_frees.fetch_add(1, std::memory_order_relaxed);
    std::free(p);
}

void operator delete[](void* p, const std::nothrow_t&) noexcept
{
    if (armed())
        g_frees.fetch_add(1, std::memory_order_relaxed);
    std::free(p);
}

void operator delete(void* p, std::align_val_t) noexcept
{
    if (armed())
        g_frees.fetch_add(1, std::memory_order_relaxed);
    std::free(p);
}

void operator delete[](void* p, std::align_val_t) noexcept
{
    if (armed())
        g_frees.fetch_add(1, std::memory_order_relaxed);
    std::free(p);
}

void operator delete(void* p, std::size_t, std::align_val_t) noexcept
{
    if (armed())
        g_frees.fetch_add(1, std::memory_order_relaxed);
    std::free(p);
}

void operator delete[](void* p, std::size_t, std::align_val_t) noexcept
{
    if (armed())
        g_frees.fetch_add(1, std::memory_order_relaxed);
    std::free(p);
}

namespace {

int failures = 0;

void expectTrue(bool condition, const char* what)
{
    if (!condition) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

lxx::dsp::SequencerSettings drumSettings()
{
    lxx::dsp::SequencerSettings settings {};
    settings.drumsDensity = { 200, 200, 200 };
    settings.drumsX = 128;
    settings.drumsY = 128;
    // The override path makes the hits deterministic: each part fires on the
    // steps whose bit is set in its pattern.
    settings.drumsOverride = 0x07;
    settings.drumsPattern[0] = 0x0005; // steps 0 and 2
    settings.drumsPattern[1] = 0x000a; // steps 1 and 3
    settings.drumsPattern[2] = 0x0014; // steps 2 and 4
    return settings;
}

void testTransportDoesNotAllocate()
{
    std::printf("Test: sequencer transport renders without allocating\n");

    lxx::dsp::SequencerTransport transport;
    transport.prepare(48000.0);
    transport.setSettings(drumSettings());
    transport.start();

    // Warm up: first calls may touch lazily initialised statics.
    for (int i = 0; i < 64; ++i)
        (void) transport.processBlock(512);

    g_allocations.store(0, std::memory_order_relaxed);
    g_frees.store(0, std::memory_order_relaxed);
    g_armed.store(true, std::memory_order_relaxed);
    unsigned long drumEvents = 0;
    for (int i = 0; i < 256; ++i) {
        const auto events = transport.processBlock(512);
        drumEvents += static_cast<unsigned long>(events.drums.size());
    }
    g_armed.store(false, std::memory_order_relaxed);

    const unsigned long allocations = g_allocations.load(std::memory_order_relaxed);
    const unsigned long frees = g_frees.load(std::memory_order_relaxed);
    std::printf("  256 blocks: allocations=%lu frees=%lu drum events=%lu\n",
                allocations, frees, drumEvents);

    expectTrue(allocations == 0, "no heap allocation while the transport renders");
    expectTrue(drumEvents > 0, "the render path actually produced drum events");
}

void testDrumSequencerDoesNotAllocate()
{
    std::printf("Test: drum sequencer step does not allocate\n");

    lxx::dsp::DrumSequencer drums;
    drums.start();
    const auto settings = drumSettings();

    uint32_t rng = 0x1234567u;
    for (int i = 0; i < 32; ++i)
        (void) drums.clockStep(settings, true, &rng);

    g_allocations.store(0, std::memory_order_relaxed);
    g_armed.store(true, std::memory_order_relaxed);
    unsigned long hits = 0;
    for (int i = 0; i < 256; ++i)
        hits += static_cast<unsigned long>(drums.clockStep(settings, true, &rng).size());
    g_armed.store(false, std::memory_order_relaxed);

    const unsigned long allocations = g_allocations.load(std::memory_order_relaxed);
    std::printf("  256 steps: allocations=%lu hits=%lu\n", allocations, hits);
    expectTrue(allocations == 0, "no heap allocation while stepping the drum sequencer");
    expectTrue(hits > 0, "the drum sequencer actually produced hits");
}

} // namespace

int main()
{
    std::printf("Realtime allocation audit\n");
    testTransportDoesNotAllocate();
    testDrumSequencerDoesNotAllocate();

    if (failures == 0)
        std::printf("test_realtime_allocation: PASS\n");
    else
        std::printf("test_realtime_allocation: %d FAIL\n", failures);
    return failures == 0 ? 0 : 1;
}
