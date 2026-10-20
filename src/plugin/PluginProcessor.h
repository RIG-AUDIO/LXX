#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "parameters/ParameterLayout.h"
#include "engines/engine_bus.h"
#include "sequencer/sequencer_router.h"
#include "sequencer/midi_drum_output.h"
#include "core/dc_blocker.h"
#include "ui/TerminalCommandRouter.h"

#include <vector>

namespace lxx {

class LXXAudioProcessor : public juce::AudioProcessor {
 public:
    LXXAudioProcessor();
    ~LXXAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& apvts() noexcept { return apvts_; }
    const juce::AudioProcessorValueTreeState& apvts() const noexcept { return apvts_; }

    // Compatibility accessors used by the editor
    dsp::SequencerTransport& sequencerTransport() noexcept { return sequencer_.legacy().transport(); }
    const dsp::SequencerTransport& sequencerTransport() const noexcept { return const_cast<LXXAudioProcessor*>(this)->sequencer_.legacy().transport(); }
    uint8_t sequencerGrooveStep() const noexcept { return transportGrooveStep_; }
    bool sequencerRunning() const noexcept { return lastSeqRun_; }
    bool sequencerRecording() const noexcept { return false; }
    double lastHostBpm() const noexcept { return lastHostBpm_; }
    dsp::DrumEngineId activeDrumEngine() const noexcept { return engineBus_.activeEngine(); }
    dsp::SequencerModeId activeSequencerMode() const noexcept { return sequencer_.activeModeId(); }
    bool sequencerOnlyActive() const noexcept { return engineBus_.sequencerOnly(); }
    uint64_t bypassedRenderCount() const noexcept { return engineBus_.bypassedRenderCount(); }
    dsp::DrumEngineBus& drumEngineBus() noexcept { return engineBus_; }
    ui::TerminalCommandRouter& commandRouter() noexcept { return commandRouter_; }

 private:
    void syncFromParameters();
    void handleTransportParameters();
    void handleMidiMessage(const juce::MidiMessage& msg);
    void dispatchMidiForChunk(const juce::MidiBuffer& midi, size_t chunkStart, size_t chunkSize);
    void applyDrumEvents(const dsp::DrumEventBuffer& events, size_t chunkStart);
    void emitMidiOutput(juce::MidiBuffer& midi, const dsp::MidiDrumOutput::MidiMessageBuffer& msgs, size_t chunkStart);
    dsp::HostClockInput captureHostClock();
    dsp::ExtendedVoiceParams buildExtendedParams() const;
    dsp::LegacyEngineParams buildLegacyParams() const;
    dsp::SequencerModeParams buildSequencerParams() const;
    dsp::VoiceMixerParams buildMixerParams() const;
    void migrateLegacyState();

    juce::AudioProcessorValueTreeState apvts_;

    double sampleRate_ = 48000.0;
    size_t maxBlock_ = 2048;
    bool prepared_ = false;
    double lastHostBpm_ = 120.0;

    dsp::DrumEngineBus engineBus_;
    dsp::SequencerRouter sequencer_;
    dsp::MidiDrumOutput midiOut_;
    dsp::DcBlocker dcLeft_;
    dsp::DcBlocker dcRight_;
    ui::TerminalCommandRouter commandRouter_;
    uint8_t transportGrooveStep_ = 0;
    bool lastSeqRun_ = false;

    std::vector<float> drumScratch_;
    dsp::DrumEventBuffer eventScratch_;
    dsp::MidiDrumOutput::MidiMessageBuffer midiScratch_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LXXAudioProcessor)
};

}  // namespace lxx
