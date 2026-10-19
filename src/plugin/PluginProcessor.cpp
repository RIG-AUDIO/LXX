#include "PluginProcessor.h"

#include <algorithm>
#include <cstddef>
#include <cmath>

#include "PluginEditor.h"
#include "core/midi_chunk.h"
#include "parameters/ParameterIDs.h"

namespace lxx {

namespace {

int drumPartForNote(int note)
{
    // Return voice as int matching DrumVoice enum
    if (note == 35) return static_cast<int>(dsp::DrumVoice::lowTom);
    if (note == 36) return static_cast<int>(dsp::DrumVoice::kick);
    if (note == 38) return static_cast<int>(dsp::DrumVoice::snare);
    if (note == 39) return static_cast<int>(dsp::DrumVoice::midPerc);
    if (note == 42) return static_cast<int>(dsp::DrumVoice::hat);
    if (note == 46) return static_cast<int>(dsp::DrumVoice::openHat);
    return -1;
}

float u8Norm(float v01) { return juce::jlimit(0.0f, 1.0f, v01); }

}  // namespace

LXXAudioProcessor::LXXAudioProcessor()
    : juce::AudioProcessor(
          BusesProperties()
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "LXX", createParameterLayout())
{
}

void LXXAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    prepared_ = false;

    const double rate = (sampleRate > 1.0 && std::isfinite(sampleRate)) ? sampleRate : 48000.0;
    const int hostBlock = samplesPerBlock > 0 ? samplesPerBlock : 512;
    const size_t maxBlock = static_cast<size_t>(juce::jmax(hostBlock, 2048));

    sampleRate_ = rate;
    maxBlock_ = maxBlock;

    engineBus_.init(static_cast<float>(rate), static_cast<uint32_t>(maxBlock));
    sequencer_.prepare(rate);
    midiOut_.prepare(rate);
    syncFromParameters();

    drumScratch_.assign(maxBlock, 0.0f);
    dcLeft_.prepare(rate);
    dcRight_.prepare(rate);

    prepared_ = true;
}

dsp::LegacyEngineParams LXXAudioProcessor::buildLegacyParams() const
{
    using namespace params;
    dsp::LegacyEngineParams p;
    p.balance = static_cast<uint8_t>(apvts_.getRawParameterValue(IDs::drumsBalance)->load() * 255.0f);
    p.bandwidth = static_cast<uint8_t>(apvts_.getRawParameterValue(IDs::drumsBandwidth)->load() * 255.0f);
    // Full engine patches when Advanced (or legacy Extended page); else tone morph.
    p.extended = false;
    if (auto* sub = dynamic_cast<juce::AudioParameterChoice*>(apvts_.getParameter(IDs::guiSubPage)))
        p.extended = sub->getIndex() >= 1;
    if (!p.extended)
        if (auto* ch = dynamic_cast<juce::AudioParameterChoice*>(apvts_.getParameter(IDs::drumPage)))
            p.extended = ch->getIndex() >= 1;
    p.tone[0] = static_cast<uint8_t>(apvts_.getRawParameterValue(IDs::bdTone)->load() * 255.0f);
    p.tone[1] = static_cast<uint8_t>(apvts_.getRawParameterValue(IDs::sdTone)->load() * 255.0f);
    p.tone[2] = static_cast<uint8_t>(apvts_.getRawParameterValue(IDs::hhTone)->load() * 255.0f);
    p.pitch[0] = static_cast<uint8_t>(juce::jlimit(0, 255, static_cast<int>(apvts_.getRawParameterValue(IDs::bdPitch)->load())));
    p.pitch[1] = static_cast<uint8_t>(juce::jlimit(0, 255, static_cast<int>(apvts_.getRawParameterValue(IDs::sdPitch)->load())));
    p.pitch[2] = static_cast<uint8_t>(juce::jlimit(0, 255, static_cast<int>(apvts_.getRawParameterValue(IDs::hhPitch)->load())));
    p.pitchDecay[0] = static_cast<uint8_t>(juce::jlimit(0, 255, static_cast<int>(apvts_.getRawParameterValue(IDs::bdPitchDecay)->load())));
    p.pitchDecay[1] = static_cast<uint8_t>(juce::jlimit(0, 255, static_cast<int>(apvts_.getRawParameterValue(IDs::sdPitchDecay)->load())));
    p.pitchMod[0] = static_cast<uint8_t>(juce::jlimit(0, 255, static_cast<int>(apvts_.getRawParameterValue(IDs::bdPitchMod)->load())));
    p.pitchMod[1] = static_cast<uint8_t>(juce::jlimit(0, 255, static_cast<int>(apvts_.getRawParameterValue(IDs::sdPitchMod)->load())));
    p.ampDecay[0] = static_cast<uint8_t>(juce::jlimit(0, 255, static_cast<int>(apvts_.getRawParameterValue(IDs::bdAmpDecay)->load())));
    p.ampDecay[1] = static_cast<uint8_t>(juce::jlimit(0, 255, static_cast<int>(apvts_.getRawParameterValue(IDs::sdAmpDecay)->load())));
    p.ampDecay[2] = static_cast<uint8_t>(juce::jlimit(0, 255, static_cast<int>(apvts_.getRawParameterValue(IDs::hhAmpDecay)->load())));
    p.crunchOrSnappy[0] = static_cast<uint8_t>(juce::jlimit(0, 255, static_cast<int>(apvts_.getRawParameterValue(IDs::bdCrunch)->load())));
    p.crunchOrSnappy[1] = static_cast<uint8_t>(juce::jlimit(0, 255, static_cast<int>(apvts_.getRawParameterValue(IDs::sdSnappy)->load())));
    return p;
}

dsp::ExtendedVoiceParams LXXAudioProcessor::buildExtendedParams() const
{
    using namespace params;
    dsp::ExtendedVoiceParams p;

    auto n01 = [this](const char* id) -> float {
        const float v = apvts_.getRawParameterValue(id)->load();
        return juce::jlimit(0.0f, 1.0f, std::isfinite(v) ? v : 0.0f);
    };
    auto u8From01 = [](float x, int lo, int hi) -> uint8_t {
        const int v = lo + static_cast<int>(std::lround(x * static_cast<float>(hi - lo)));
        return static_cast<uint8_t>(juce::jlimit(lo, hi, v));
    };

    // LOW — firmware-style pitch/decay indices derived from 0..1 knobs
    p.lowPitch = u8From01(n01(IDs::lowPitch), 28, 72);
    p.lowAmpDecay = u8From01(n01(IDs::lowDecay), 80, 220);
    p.lowPitchDecay = u8From01(n01(IDs::lowDecay), 20, 60);
    p.lowPitchMod = u8From01(1.0f - n01(IDs::lowTone) * 0.35f, 90, 160);
    p.lowTone = u8From01(n01(IDs::lowTone), 40, 200);
    p.lowCharacter = n01(IDs::lowTone); // continuous boom↔tom colour within forced mode
    p.lowMode = 0;
    if (auto* ch = dynamic_cast<juce::AudioParameterChoice*>(apvts_.getParameter(IDs::lowCharacter)))
        p.lowMode = juce::jlimit(0, 2, ch->getIndex());
    p.lowLevel = 0.55f;

    p.midPitch = u8From01(n01(IDs::midPitch), 70, 140);
    p.midAmpDecay = u8From01(n01(IDs::midDecay), 40, 160);
    p.midPitchDecay = u8From01(n01(IDs::midDecay), 12, 48);
    p.midPitchMod = u8From01(n01(IDs::midTone), 40, 140);
    p.midTone = u8From01(n01(IDs::midTone), 60, 220);
    p.midLevel = 0.42f;

    // Open hat — sibling of closed (hhPitch); longer amp env via smaller LUT index
    const float ohDecay01 = n01(IDs::ohDecay);
    p.ohPitch = static_cast<uint8_t>(juce::jlimit(0, 255,
        static_cast<int>(apvts_.getRawParameterValue(IDs::hhPitch)->load()) - 8));
    p.ohAmpDecay = u8From01(1.0f - ohDecay01, 8, 90); // longer when knob high
    p.ohTone = u8From01(n01(IDs::ohTone), 64, 200);
    p.ohLevel = 0.32f;
    return p;
}

dsp::SequencerModeParams LXXAudioProcessor::buildSequencerParams() const
{
    using namespace params;
    dsp::SequencerModeParams p;
    int mode = 0;
    if (auto* ch = dynamic_cast<juce::AudioParameterChoice*>(apvts_.getParameter(IDs::sequencerMode)))
        mode = ch->getIndex();
    p.mode = static_cast<dsp::SequencerModeId>(juce::jlimit(0, 5, mode));
    p.tempo = static_cast<uint8_t>(juce::jlimit(40.0f, 240.0f, apvts_.getRawParameterValue(IDs::tempo)->load()));
    p.hostSync = apvts_.getRawParameterValue(IDs::hostSync)->load() >= 0.5f;
    int clockDiv = 4;
    if (auto* ch = dynamic_cast<juce::AudioParameterChoice*>(apvts_.getParameter(IDs::clockDivision)))
        clockDiv = ch->getIndex();
    p.clockDivision = static_cast<uint8_t>(juce::jlimit(0, 5, clockDiv));
    p.swing = static_cast<uint8_t>(apvts_.getRawParameterValue(IDs::swing)->load() * 127.0f);
    p.drumsX = static_cast<uint8_t>(apvts_.getRawParameterValue(IDs::drumsX)->load() * 255.0f);
    p.drumsY = static_cast<uint8_t>(apvts_.getRawParameterValue(IDs::drumsY)->load() * 255.0f);
    p.density[0] = static_cast<uint8_t>(apvts_.getRawParameterValue(IDs::bdDensity)->load() * 255.0f);
    p.density[1] = static_cast<uint8_t>(apvts_.getRawParameterValue(IDs::sdDensity)->load() * 255.0f);
    p.density[2] = static_cast<uint8_t>(apvts_.getRawParameterValue(IDs::hhDensity)->load() * 255.0f);
    p.gridsChaos = static_cast<uint8_t>(apvts_.getRawParameterValue(IDs::gridsChaos)->load() * 255.0f);
    p.technoDensity = u8Norm(apvts_.getRawParameterValue(IDs::technoDensity)->load());
    p.technoVariation = u8Norm(apvts_.getRawParameterValue(IDs::technoVariation)->load());
    p.technoSyncopation = u8Norm(apvts_.getRawParameterValue(IDs::technoSyncopation)->load());
    p.technoFill = u8Norm(apvts_.getRawParameterValue(IDs::technoFill)->load());
    p.technoSwing = u8Norm(apvts_.getRawParameterValue(IDs::technoSwing)->load());
    p.technoSeed = 1;
    if (auto* seedP = dynamic_cast<juce::AudioParameterInt*>(apvts_.getParameter(IDs::technoSeed)))
        p.technoSeed = static_cast<uint32_t>(juce::jlimit(1, 9999, seedP->get()));
    p.running = lastSeqRun_;
    return p;
}

void LXXAudioProcessor::syncFromParameters()
{
    using namespace params;
    engineBus_.requestEngine(dsp::DrumEngineId::native);
    engineBus_.setLegacyParams(buildLegacyParams());
    engineBus_.setExtendedParams(buildExtendedParams());
    engineBus_.setMixerParams(buildMixerParams());

    auto seqParams = buildSequencerParams();
    sequencer_.setParams(seqParams);

    const bool seqOnly = apvts_.getRawParameterValue(IDs::seqOnly)->load() >= 0.5f;
    engineBus_.setSequencerOnly(seqOnly);
    const bool midiWanted = seqOnly || apvts_.getRawParameterValue(IDs::midiOut)->load() >= 0.5f;
    midiOut_.setEnabled(midiWanted);
}

dsp::VoiceMixerParams LXXAudioProcessor::buildMixerParams() const
{
    using namespace params;
    dsp::VoiceMixerParams m;
    auto db = [this](const char* id) -> float {
        if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(apvts_.getParameter(id)))
            return p->get();
        return 0.0f;
    };
    auto muted = [this](const char* id) {
        return apvts_.getRawParameterValue(id)->load() >= 0.5f;
    };
    m.volDb[0] = db(IDs::volKick);
    m.volDb[1] = db(IDs::volSnare);
    m.volDb[2] = db(IDs::volCh);
    m.volDb[3] = db(IDs::volLow);
    m.volDb[4] = db(IDs::volMid);
    m.volDb[5] = db(IDs::volOh);
    m.mute[0] = muted(IDs::muteKick);
    m.mute[1] = muted(IDs::muteSnare);
    m.mute[2] = muted(IDs::muteCh);
    m.mute[3] = muted(IDs::muteLow);
    m.mute[4] = muted(IDs::muteMid);
    m.mute[5] = muted(IDs::muteOh);
    return m;
}

void LXXAudioProcessor::handleTransportParameters()
{
    using namespace params;
    const bool run = apvts_.getRawParameterValue(IDs::seqRun)->load() >= 0.5f;
    if (run != lastSeqRun_) {
        lastSeqRun_ = run;
        sequencer_.setRunning(run);
        if (!run) {
            dsp::MidiDrumOutput::MidiMessageBuffer flush;
            midiOut_.flushAll(flush, 0);
        }
    }
}

void LXXAudioProcessor::releaseResources()
{
    prepared_ = false;
    dcLeft_.reset();
    dcRight_.reset();
    midiOut_.reset();
}

bool LXXAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.inputBuses.size() != 0)
        return false;
    if (layouts.outputBuses.size() != 1)
        return false;
    const auto main = layouts.getMainOutputChannelSet();
    return main == juce::AudioChannelSet::stereo()
        || main == juce::AudioChannelSet::mono();
}

void LXXAudioProcessor::applyDrumEvents(const dsp::DrumEventBuffer& events, size_t /*chunkStart*/)
{
    if (engineBus_.sequencerOnly())
        return;
    for (uint32_t i = 0; i < events.count; ++i) {
        const auto& e = events.events[i];
        engineBus_.trigger(e.voice, e.velocity, e.lowArticulation);
    }
}

void LXXAudioProcessor::emitMidiOutput(juce::MidiBuffer& midi,
                                           const dsp::MidiDrumOutput::MidiMessageBuffer& msgs,
                                           size_t chunkStart)
{
    for (uint32_t i = 0; i < msgs.count; ++i) {
        const auto& m = msgs.msgs[i];
        const int pos = static_cast<int>(chunkStart + m.sampleOffset);
        if (m.isNoteOn)
            midi.addEvent(juce::MidiMessage::noteOn(1, m.note, m.velocity), pos);
        else
            midi.addEvent(juce::MidiMessage::noteOff(1, m.note), pos);
    }
}

void LXXAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // Collect outgoing MIDI separately; keep input for triggering.
    juce::MidiBuffer incoming = midi;
    midi.clear();

    if (!prepared_)
        return;

    const auto numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || buffer.getNumChannels() <= 0)
        return;

    syncFromParameters();
    handleTransportParameters();

    const auto hostClock = captureHostClock();
    const float volume = juce::jlimit(0.0f, 1.0f,
        apvts_.getRawParameterValue(params::IDs::volume)->load());

    if (drumScratch_.empty())
        return;

    size_t rendered = 0;
    const size_t total = static_cast<size_t>(numSamples);
    const size_t capacity = drumScratch_.size();

    while (rendered < total) {
        const size_t chunk = dsp::nextHostChunkSize(total - rendered, capacity);
        if (chunk == 0)
            break;

        dispatchMidiForChunk(incoming, rendered, chunk);

        dsp::HostClockInput chunkHost = hostClock;
        if (chunkHost.hasPpq && chunkHost.enabled) {
            const double ppqPerSample = (chunkHost.bpm > 1.0 ? chunkHost.bpm : 120.0)
                / (60.0 * (chunkHost.sampleRate > 1.0 ? chunkHost.sampleRate : sampleRate_));
            chunkHost.ppq = hostClock.ppq + ppqPerSample * static_cast<double>(rendered);
        }

        eventScratch_.clear();
        sequencer_.process(static_cast<uint32_t>(chunk), chunkHost, eventScratch_);
        transportGrooveStep_ = sequencer_.grooveStep();

        // Adjust event offsets to be relative to chunk (already are); trigger engines.
        applyDrumEvents(eventScratch_, rendered);

        midiOut_.process(eventScratch_, static_cast<uint32_t>(chunk), midiScratch_);
        emitMidiOutput(midi, midiScratch_, rendered);

        if (lastSeqRun_ || engineBus_.isPlaying())
            engineBus_.processBlock(drumScratch_.data(), static_cast<uint32_t>(chunk));
        else
            std::fill_n(drumScratch_.data(), chunk, 0.0f);

        constexpr float kDrumBusGain = 0.55f;
        for (size_t i = 0; i < chunk; ++i) {
            float sample = drumScratch_[i] * kDrumBusGain * volume;
            if (!std::isfinite(sample))
                sample = 0.0f;
            drumScratch_[i] = sample;
        }

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
            auto* dest = buffer.getWritePointer(channel, static_cast<int>(rendered));
            dsp::DcBlocker& dc = (channel == 0) ? dcLeft_ : dcRight_;
            for (size_t i = 0; i < chunk; ++i) {
                float sample = dc.process(drumScratch_[i]);
                dest[i] = std::isfinite(sample) ? sample : 0.0f;
            }
        }

        rendered += chunk;
    }
}

void LXXAudioProcessor::handleMidiMessage(const juce::MidiMessage& msg)
{
    if (msg.isNoteOn()) {
        const auto note = static_cast<uint8_t>(juce::jlimit<int>(0, 127, msg.getNoteNumber()));
        const int part = drumPartForNote(note);
        if (part < 0)
            return;
        const auto vel = static_cast<uint8_t>(juce::jlimit<int>(0, 127, msg.getVelocity()));
        if (vel <= 0)
            return;
        engineBus_.trigger(static_cast<dsp::DrumVoice>(part), static_cast<float>(vel) / 127.0f);
        return;
    }

    if (msg.isAllSoundOff()) {
        engineBus_.reset();
        sequencer_.reset();
        midiOut_.reset();
        return;
    }

    if (msg.isAllNotesOff()) {
        dsp::MidiDrumOutput::MidiMessageBuffer flush;
        midiOut_.flushAll(flush, 0);
        return;
    }
}

void LXXAudioProcessor::dispatchMidiForChunk(const juce::MidiBuffer& midi, size_t chunkStart, size_t chunkSize)
{
    for (const auto metadata : midi) {
        if (!dsp::midiEventInChunk(metadata.samplePosition, chunkStart, chunkSize))
            continue;
        handleMidiMessage(metadata.getMessage());
    }
}

juce::AudioProcessorEditor* LXXAudioProcessor::createEditor()
{
    return new LXXAudioProcessorEditor(*this);
}

dsp::HostClockInput LXXAudioProcessor::captureHostClock()
{
    using namespace params;
    dsp::HostClockInput host;
    host.sampleRate = sampleRate_;
    host.enabled = apvts_.getRawParameterValue(IDs::hostSync)->load() >= 0.5f;
    host.divisionIndex = static_cast<uint8_t>(
        juce::jlimit(0, 5, static_cast<int>(apvts_.getRawParameterValue(IDs::clockDivision)->load())));
    host.bpm = lastHostBpm_;

    if (auto* head = getPlayHead()) {
        if (const auto position = head->getPosition()) {
            host.isPlaying = position->getIsPlaying();
            if (const auto bpm = position->getBpm()) {
                if (std::isfinite(*bpm) && *bpm > 1.0) {
                    host.bpm = juce::jlimit(20.0, 400.0, *bpm);
                    lastHostBpm_ = host.bpm;
                }
            }
            if (const auto ppq = position->getPpqPosition()) {
                host.ppq = *ppq;
                host.hasPpq = std::isfinite(*ppq) && *ppq >= 0.0;
            }
        }
    }
    return host;
}

void LXXAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts_.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void LXXAudioProcessor::migrateLegacyState()
{
    using namespace params;
    // Force obsolete Analog engine choice → Native.
    if (auto* eng = dynamic_cast<juce::AudioParameterChoice*>(apvts_.getParameter(IDs::drumEngine))) {
        if (eng->getIndex() != 0) {
            eng->beginChangeGesture();
            eng->setValueNotifyingHost(eng->convertTo0to1(0.0f));
            eng->endChangeGesture();
        }
    }
    // Old ui_page was GUI/CLI (0/1). Keep as-is; new middle GUI+CLI is additive.
}

void LXXAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts_.state.getType())) {
            apvts_.replaceState(juce::ValueTree::fromXml(*xml));
            migrateLegacyState();
        }
}

}  // namespace lxx

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new lxx::LXXAudioProcessor();
}
