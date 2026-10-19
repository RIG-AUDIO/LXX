#include "core/midi_chunk.h"

#include <cstdio>
#include <vector>

namespace {

int failures = 0;

void expectTrue(bool cond, const char* msg)
{
    if (!cond) {
        std::printf("FAIL: %s\n", msg);
        ++failures;
    }
}

struct HostEvent {
    int offset = 0;
    int id = 0;
};

struct EngineEvent {
    int hostOffset = 0;
    int localOffset = 0;
    size_t chunkIndex = 0;
    size_t chunkStart = 0;
    size_t chunkSize = 0;
    int id = 0;
};

std::vector<EngineEvent> dispatch(size_t hostBlock, size_t capacity, const std::vector<HostEvent>& host)
{
    std::vector<EngineEvent> engine;
    size_t rendered = 0;
    size_t chunkIndex = 0;
    while (rendered < hostBlock) {
        const size_t chunk = lxx::dsp::nextHostChunkSize(hostBlock - rendered, capacity);
        if (chunk == 0)
            break;
        for (const auto& ev : host) {
            if (!lxx::dsp::midiEventInChunk(ev.offset, rendered, chunk))
                continue;
            EngineEvent out;
            out.hostOffset = ev.offset;
            out.localOffset = lxx::dsp::midiLocalOffset(ev.offset, rendered);
            out.chunkIndex = chunkIndex;
            out.chunkStart = rendered;
            out.chunkSize = chunk;
            out.id = ev.id;
            engine.push_back(out);
        }
        rendered += chunk;
        ++chunkIndex;
    }
    return engine;
}

void testBlock(size_t hostBlock)
{
    const size_t capacity = 2048;
    std::vector<HostEvent> host;
    int id = 0;
    auto add = [&](int offset) {
        if (offset >= 0 && static_cast<size_t>(offset) < hostBlock)
            host.push_back({ offset, id++ });
    };

    add(0);
    add(static_cast<int>(hostBlock) - 1);
    const int chunk = static_cast<int>(lxx::dsp::kHostProcessChunk);
    for (int b = chunk - 1; b <= chunk + 1; ++b)
        add(b);
    for (int b = 2 * chunk - 1; b <= 2 * chunk + 1; ++b)
        add(b);
    add(static_cast<int>(hostBlock) / 2);

    const auto engine = dispatch(hostBlock, capacity, host);
    expectTrue(engine.size() == host.size(), "host events == engine events");

    std::vector<int> seen(static_cast<int>(host.size()), 0);
    for (const auto& e : engine) {
        if (e.id >= 0 && e.id < static_cast<int>(seen.size()))
            ++seen[static_cast<size_t>(e.id)];
        expectTrue(e.localOffset >= 0, "local offset >= 0");
        expectTrue(static_cast<size_t>(e.localOffset) < e.chunkSize, "local offset < chunk size");
        expectTrue(e.hostOffset == static_cast<int>(e.chunkStart) + e.localOffset,
                   "local = host - chunkStart");
        expectTrue(lxx::dsp::midiEventInChunk(e.hostOffset, e.chunkStart, e.chunkSize),
                   "event belongs to its chunk");
    }
    for (int n : seen)
        expectTrue(n == 1, "each host event dispatched exactly once");
}

} // namespace

int main()
{
    const size_t sizes[] = { 1, 2, 7, 31, 32, 33, 63, 64, 65, 127, 128, 129,
                             256, 511, 512, 1024, 2048, 4096 };
    for (size_t n : sizes)
        testBlock(n);

    {
        const auto engine = dispatch(64, 2048, { { 40, 0 } });
        expectTrue(engine.size() == 1, "event at 40 dispatched once in 64-sample block");
        if (!engine.empty()) {
            expectTrue(engine[0].chunkStart == 32, "event 40 belongs to chunk starting at 32");
            expectTrue(engine[0].localOffset == 8, "local offset 40-32=8");
            expectTrue(engine[0].chunkSize == 32, "second chunk is 32 samples");
        }
    }

    {
        const auto engine = dispatch(64, 2048, { { 31, 0 }, { 32, 1 } });
        expectTrue(engine.size() == 2, "boundary pair 31/32 both dispatched");
        if (engine.size() == 2) {
            expectTrue(engine[0].chunkStart == 0 && engine[0].localOffset == 31, "31 in first chunk");
            expectTrue(engine[1].chunkStart == 32 && engine[1].localOffset == 0, "32 in second chunk");
        }
    }

    if (failures == 0)
        std::printf("test_midi_chunking: PASS\n");
    else
        std::printf("test_midi_chunking: %d FAIL\n", failures);
    return failures == 0 ? 0 : 1;
}
