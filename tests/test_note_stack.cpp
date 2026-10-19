#include "voice/note_stack.h"

#include <chrono>
#include <cstdio>
#include <cstdint>

namespace {

int failures = 0;

void expectTrue(bool cond, const char* msg)
{
    if (!cond) {
        std::printf("FAIL: %s\n", msg);
        ++failures;
    }
}

} // namespace

int main()
{
    using lxx::dsp::NoteStack;
    NoteStack<16> stack;
    stack.init();

    stack.noteOn(60, 100);
    expectTrue(stack.size() == 1 && stack.mostRecentNote() == 60, "single note on");
    stack.noteOff(60);
    expectTrue(stack.empty(), "note off clears stack");

    stack.noteOff(61);
    expectTrue(stack.empty(), "unmatched note-off is safe");

    stack.noteOn(60, 100);
    stack.noteOn(64, 90);
    expectTrue(stack.mostRecentNote() == 64, "LIFO top is 64");
    stack.noteOff(64);
    expectTrue(stack.mostRecentNote() == 60, "release top returns to 60");
    stack.noteOff(60);

    stack.noteOn(60, 100);
    stack.noteOn(64, 90);
    stack.noteOn(67, 80);
    stack.noteOff(60);
    expectTrue(stack.size() == 2 && stack.mostRecentNote() == 67, "release bottom keeps top");
    stack.noteOff(67);
    expectTrue(stack.mostRecentNote() == 64, "then 64");
    stack.clear();

    for (int i = 0; i < 40; ++i)
        stack.noteOn(static_cast<uint8_t>(i), 100);
    expectTrue(stack.size() == 16, "capacity capped at 16");
    stack.noteOn(90, 110);
    expectTrue(stack.size() == 16, "overflow insert keeps size 16");
    expectTrue(stack.mostRecentNote() == 90, "newest note is top after eviction");

    for (int i = 0; i < 200; ++i)
        stack.noteOff(static_cast<uint8_t>(i));
    expectTrue(stack.empty(), "excess note-offs cannot underflow");

    stack.noteOn(60, 100);
    stack.noteOn(60, 80);
    expectTrue(stack.size() == 1, "duplicate note-on replaces, no accumulation");
    stack.clear();

    uint32_t rng = 0xC0FFEEu;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 200000; ++i) {
        rng = rng * 1664525u + 1013904223u;
        const uint8_t note = static_cast<uint8_t>(rng & 127u);
        if ((rng >> 8) & 1u)
            stack.noteOn(note, 100);
        else
            stack.noteOff(note);
        expectTrue(stack.size() <= 16, "size never exceeds capacity");
        if (stack.empty())
            expectTrue(true, "empty ok");
        else
            expectTrue(stack.mostRecentNote() <= 127, "top note in range");
    }
    auto t1 = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    expectTrue(ms < 5000.0, "200k stack ops finished (no hang)");

    stack.clear();
    for (int n = 0; n < 128; ++n)
        stack.noteOn(static_cast<uint8_t>(n), 1);
    expectTrue(stack.size() == 16, "128 distinct notes still cap at 16");

    if (failures == 0)
        std::printf("test_note_stack: PASS\n");
    else
        std::printf("test_note_stack: %d FAIL\n", failures);
    return failures == 0 ? 0 : 1;
}
