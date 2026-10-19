#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/LXXUiPalette.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <memory>
#include <vector>

namespace {

int failures = 0;
using Clock = std::chrono::steady_clock;

void expectTrue(bool cond, const char* msg)
{
    if (!cond) {
        std::printf("FAIL: %s\n", msg);
        ++failures;
    }
}

double seconds(Clock::time_point a, Clock::time_point b)
{
    return std::chrono::duration<double>(b - a).count();
}

void process(lxx::LXXAudioProcessor& p, juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi)
{
    auto t0 = Clock::now();
    p.processBlock(buf, midi);
    midi.clear();
    const double wall = seconds(t0, Clock::now());
    if (wall > 0.1) {
        std::printf("FAIL: processBlock watchdog %.3fs samples=%d\n", wall, buf.getNumSamples());
        ++failures;
    }
}

bool bufferFinite(const juce::AudioBuffer<float>& buf)
{
    for (int c = 0; c < buf.getNumChannels(); ++c) {
        const float* d = buf.getReadPointer(c);
        for (int i = 0; i < buf.getNumSamples(); ++i) {
            if (!std::isfinite(d[i]))
                return false;
        }
    }
    return true;
}

void addNote(juce::MidiBuffer& midi, int sample, int note, bool on, int vel = 100)
{
    if (on)
        midi.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(vel)), sample);
    else
        midi.addEvent(juce::MidiMessage::noteOff(1, note), sample);
}

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    constexpr int kSr = 48000;
    constexpr int kBlock = 64;

    auto proc = std::make_unique<lxx::LXXAudioProcessor>();
    proc->prepareToPlay(kSr, kBlock);
    juce::AudioBuffer<float> buf(2, kBlock);
    juce::MidiBuffer midi;

    {
        for (int c = 0; c < buf.getNumChannels(); ++c)
            juce::FloatVectorOperations::fill(buf.getWritePointer(c), 0.73f, kBlock);
        process(*proc, buf, midi);
        float peak = 0.0f;
        int nonzero = 0;
        for (int c = 0; c < buf.getNumChannels(); ++c) {
            const float* d = buf.getReadPointer(c);
            for (int i = 0; i < kBlock; ++i) {
                peak = std::max(peak, std::abs(d[i]));
                if (d[i] != 0.0f)
                    ++nonzero;
            }
        }
        std::printf("idle peak=%.8f nonzero=%d\n", peak, nonzero);
        expectTrue(peak == 0.0f && nonzero == 0, "default idle host output is exact silence");
        expectTrue(bufferFinite(buf), "idle finite");
    }

    const int sentinelSizes[] = { 31, 32, 33, 63, 64, 65, 127, 128, 129, 511, 512, 513, 1024, 2048 };
    for (int n : sentinelSizes) {
        auto p = std::make_unique<lxx::LXXAudioProcessor>();
        p->prepareToPlay(kSr, n);
        juce::AudioBuffer<float> dirty(2, n);
        for (int c = 0; c < 2; ++c)
            juce::FloatVectorOperations::fill(dirty.getWritePointer(c), 0.91f, n);
        juce::MidiBuffer empty;
        process(*p, dirty, empty);
        bool clean = true;
        for (int c = 0; c < 2 && clean; ++c) {
            const float* d = dirty.getReadPointer(c);
            for (int i = 0; i < n; ++i) {
                if (d[i] != 0.0f)
                    clean = false;
            }
        }
        expectTrue(clean, "chunk sentinel: idle overwrites every host sample");
    }

    const int seq[] = { 60, 64, 67 };
    for (int n : seq) {
        addNote(midi, 0, n, true);
        process(*proc, buf, midi);
        expectTrue(bufferFinite(buf), "finite after note on");
        for (int h = 0; h < 8; ++h)
            process(*proc, buf, midi);
        addNote(midi, kBlock - 1, n, false);
        process(*proc, buf, midi);
        expectTrue(bufferFinite(buf), "finite after note off");
    }

    addNote(midi, 0, 60, true);
    addNote(midi, 16, 64, true);
    addNote(midi, 32, 60, false);
    addNote(midi, 40, 67, true);
    addNote(midi, 48, 64, false);
    addNote(midi, 63, 67, false);
    process(*proc, buf, midi);
    expectTrue(bufferFinite(buf), "overlap pattern finite");

    int transitions = 0;
    uint32_t rng = 0xDEADu;
    auto t0 = Clock::now();
    for (int i = 0; i < 120000; ++i) {
        rng = rng * 1664525u + 1013904223u;
        const int note = static_cast<int>(40 + (rng % 48));
        const int offset = static_cast<int>(rng % kBlock);
        if ((rng >> 8) & 1u)
            addNote(midi, offset, note, true, static_cast<int>(1 + (rng % 127)));
        else
            addNote(midi, offset, note, false);
        if ((i % 64) == 0)
            midi.addEvent(juce::MidiMessage::allNotesOff(1), 0);
        process(*proc, buf, midi);
        ++transitions;
        if (!bufferFinite(buf)) {
            expectTrue(false, "non-finite during processor stress");
            break;
        }
        if (seconds(t0, Clock::now()) > 180.0) {
            expectTrue(false, "processor stress watchdog");
            break;
        }
    }
    std::printf("processor note transitions=%d\n", transitions);
    expectTrue(transitions >= 100000, "100k+ processor transitions");

    auto a = std::make_unique<lxx::LXXAudioProcessor>();
    auto b = std::make_unique<lxx::LXXAudioProcessor>();
    a->prepareToPlay(44100.0, 32);
    b->prepareToPlay(48000.0, 128);
    juce::AudioBuffer<float> ba(2, 32), bb(2, 128);
    juce::MidiBuffer ma, mb;
    addNote(ma, 0, 50, true);
    addNote(mb, 40, 80, true);
    process(*a, ba, ma);
    process(*b, bb, mb);
    a.reset();
    process(*b, bb, mb);
    expectTrue(bufferFinite(bb), "processor B survives A destroy");
    a = std::make_unique<lxx::LXXAudioProcessor>();
    a->prepareToPlay(96000.0, 64);
    juce::AudioBuffer<float> bc(2, 64);
    process(*a, bc, ma);
    process(*b, bb, mb);
    expectTrue(bufferFinite(bc) && bufferFinite(bb), "A recreate + B ok");

    for (int i = 0; i < 200; ++i) {
        auto p = std::make_unique<lxx::LXXAudioProcessor>();
        const double sr = (i % 3 == 0) ? 44100.0 : (i % 3 == 1) ? 48000.0 : 96000.0;
        const int block = 7 + (i % 512);
        p->prepareToPlay(sr, block);
        juce::AudioBuffer<float> tmp(2, block);
        juce::MidiBuffer m;
        addNote(m, 0, 60 + (i % 12), true);
        process(*p, tmp, m);
        juce::MemoryBlock state;
        p->getStateInformation(state);
        p->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        p->releaseResources();
        p->prepareToPlay(48000.0, 64);
        juce::AudioBuffer<float> tmp2(2, 64);
        process(*p, tmp2, m);
        if ((i % 40) == 0) {
            std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
            ed.reset();
        }
        expectTrue(bufferFinite(tmp2), "lifecycle finite");
    }

    {
        auto p = std::make_unique<lxx::LXXAudioProcessor>();
        p->prepareToPlay(48000.0, 64);
        juce::AudioBuffer<float> held(2, 64);
        juce::MidiBuffer idle;
        std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
        auto* lxxEd = dynamic_cast<lxx::LXXAudioProcessorEditor*>(ed.get());
        expectTrue(lxxEd != nullptr, "editor type");
        if (lxxEd != nullptr) {
            lxxEd->setSkinForTests(lxx::ui::SkinId::terminal);
            process(*p, held, idle);
            lxxEd->setSkinForTests(lxx::ui::SkinId::raytracing);
            lxxEd->setGuiSizeForTests(lxx::ui::GuiSize::small);
            process(*p, held, idle);
            lxxEd->setSkinForTests(lxx::ui::SkinId::raytracingPhoto);
            lxxEd->setGuiSizeForTests(lxx::ui::GuiSize::large);
            process(*p, held, idle);
            lxxEd->setSkinForTests(lxx::ui::SkinId::defaultSkin);
            lxxEd->setGuiSizeForTests(lxx::ui::GuiSize::medium);
            process(*p, held, idle);
            expectTrue(lxxEd->skinForTests() == lxx::ui::SkinId::defaultSkin,
                       "Default skin restored");
            expectTrue(lxxEd->getWidth() == lxx::ui::Layout::designWidth
                           && lxxEd->getHeight() == lxx::ui::Layout::designHeight,
                       "default GUI design size");
            lxxEd->setSize(lxx::ui::Layout::minWidth, lxx::ui::Layout::minHeight);
            expectTrue(lxxEd->getWidth() == lxx::ui::Layout::minWidth, "resizable min width");
            lxxEd->setSize(lxx::ui::Layout::designWidth, lxx::ui::Layout::designHeight);
            if (auto* sync = p->apvts().getParameter("host_sync")) {
                const float before = sync->getValue();
                sync->setValueNotifyingHost(0.0f);
                process(*p, held, idle);
                sync->setValueNotifyingHost(1.0f);
                process(*p, held, idle);
                sync->setValueNotifyingHost(before);
                process(*p, held, idle);
            }
        }
        float skinPeak = 0.0f;
        for (int c = 0; c < held.getNumChannels(); ++c) {
            const float* d = held.getReadPointer(c);
            for (int i = 0; i < held.getNumSamples(); ++i)
                skinPeak = std::max(skinPeak, std::abs(d[i]));
        }
        expectTrue(skinPeak == 0.0f, "skin/scale switch leaves idle silence");
        expectTrue(bufferFinite(held), "skin switch idle audio finite");
        ed.reset();
        p->releaseResources();
    }

    proc->releaseResources();
    proc.reset();

    if (failures == 0)
        std::printf("LXXHostRuntimeTest: PASS\n");
    else
        std::printf("LXXHostRuntimeTest: %d FAIL\n", failures);
    return failures == 0 ? 0 : 1;
}
