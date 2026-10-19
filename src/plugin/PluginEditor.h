#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>

#include "PluginProcessor.h"
#include "ui/LXXKnob.h"
#include "ui/LXXLookAndFeel.h"
#include "ui/LXXSkin.h"
#include "ui/CommandConsole.h"
#include "ui/SeqGridComponent.h"

namespace lxx {

class LXXAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer {
 public:
    explicit LXXAudioProcessorEditor(LXXAudioProcessor&);
    ~LXXAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setSkinForTests(ui::SkinId);
    void setGuiSizeForTests(ui::GuiSize);
    ui::SkinId skinForTests() const noexcept;

 private:
    void attachAllControls();
    void attachKnob(ui::LXXKnob& knob, const char* paramId);
    void placeKnob(ui::LXXKnob& knob, juce::Rectangle<int> cell);
    void placeKnob(ui::LXXKnob& knob, int x, int y, int w, int h);
    void drawSectionFrame(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title) const;
    void layoutAdvancedVoice(juce::Rectangle<int> area,
                             ui::LXXKnob* knobs[], int nKnobs,
                             ui::LXXKnob& vol, juce::ToggleButton& mute, juce::ToggleButton& lock,
                             juce::Component* extra = nullptr);
    void layoutKnobRow(juce::Rectangle<int> area, ui::LXXKnob* knobs[], int n, int knobSize);
    void runCli(const juce::String& line);
    void timerCallback() override;
    void refreshVisibility();
    void enforceSeqOnlyMidi();
    int uiMode() const;
    bool isAdvanced() const;
    int seqMode() const;
    bool extendedSeq() const;
    bool legacyDensitiesActive() const;
    bool mapXYActive() const;
    bool technoMacrosActive() const;

    LXXAudioProcessor& processor_;
    bool layoutReady_ = false;
    ui::LXXLookAndFeel lookAndFeel_;
    juce::ComponentBoundsConstrainer constrainer_;

    struct DesignSurface : public juce::Component {
        LXXAudioProcessorEditor* owner = nullptr;
        void paint(juce::Graphics& g) override;
        juce::Rectangle<int> headerArea, soundArea, genArea, transportArea, globalArea, stepArea, locksArea;
        juce::Rectangle<int> advKick, advLow, advSnare, advPerc, advCh, advOh, advTransport;
    };
    DesignSurface designSurface_;
    ui::CommandConsole commandConsole_;

    juce::ComboBox uiPageBox_;
    juce::ComboBox guiSubBox_;
    juce::ToggleButton hostSyncToggle_ { "HOST SYNC" };
    juce::ToggleButton midiOutToggle_ { "MIDI OUT" };
    juce::ToggleButton runToggle_ { "RUN" };
    juce::ToggleButton seqOnlyToggle_ { "SEQ ONLY" };
    juce::ComboBox clockDivisionBox_;
    juce::ComboBox drumPageBox_;
    juce::ComboBox sequencerModeBox_;
    juce::ComboBox lowCharacterBox_;

    juce::TextButton generateBtn_ { "GENERATE" };
    juce::TextButton mutateBtn_ { "MUTATE" };
    juce::TextButton randomizeKitBtn_ { "RAND KIT" };

    juce::Label locksLabel_ { {}, "LOCK" };
    juce::ToggleButton muteKick_ { "M" };
    juce::ToggleButton muteSnare_ { "M" };
    juce::ToggleButton muteCh_ { "M" };
    juce::ToggleButton muteLow_ { "M" };
    juce::ToggleButton muteMid_ { "M" };
    juce::ToggleButton muteOh_ { "M" };
    juce::ToggleButton lockKick_ { "K" };
    juce::ToggleButton lockSnare_ { "S" };
    juce::ToggleButton lockCh_ { "CH" };
    juce::ToggleButton lockLow_ { "LOW" };
    juce::ToggleButton lockMid_ { "MID" };
    juce::ToggleButton lockOh_ { "OH" };

    ui::LXXKnob drumsX_ { "MAP X" };
    ui::LXXKnob drumsY_ { "MAP Y" };
    ui::LXXKnob bdDensity_ { "BD DENS" };
    ui::LXXKnob sdDensity_ { "SD DENS" };
    ui::LXXKnob hhDensity_ { "HH DENS" };
    ui::LXXKnob bdTone_ { "TONE" };
    ui::LXXKnob sdTone_ { "TONE" };
    ui::LXXKnob hhTone_ { "TONE" };
    ui::LXXKnob drumsBalance_ { "BALANCE" };
    ui::LXXKnob drumsBandwidth_ { "BITCRUSH" };
    ui::LXXKnob bdPitch_ { "PITCH" };
    ui::LXXKnob bdPitchDecay_ { "P DECAY" };
    ui::LXXKnob bdPitchMod_ { "P ENV" };
    ui::LXXKnob bdAmpDecay_ { "DECAY" };
    ui::LXXKnob bdCrunch_ { "CRUNCH" };
    ui::LXXKnob sdPitch_ { "PITCH" };
    ui::LXXKnob sdPitchDecay_ { "P DECAY" };
    ui::LXXKnob sdPitchMod_ { "P ENV" };
    ui::LXXKnob sdAmpDecay_ { "DECAY" };
    ui::LXXKnob sdSnappy_ { "SNAPPY" };
    ui::LXXKnob hhPitch_ { "PITCH" };
    ui::LXXKnob hhAmpDecay_ { "DECAY" };
    ui::LXXKnob gridsChaos_ { "CHAOS" };
    ui::LXXKnob technoDensity_ { "DENSITY" };
    ui::LXXKnob technoVariation_ { "VARIATION" };
    ui::LXXKnob technoSyncopation_ { "SYNC" };
    ui::LXXKnob technoFill_ { "FILL" };
    ui::LXXKnob technoSwing_ { "T-SWING" };
    ui::LXXKnob technoSeed_ { "SEED" };
    ui::LXXKnob ohDecay_ { "DECAY" };
    ui::LXXKnob ohTone_ { "TONE" };
    ui::LXXKnob lowPitch_ { "PITCH" };
    ui::LXXKnob lowDecay_ { "DECAY" };
    ui::LXXKnob lowTone_ { "CHAR" };
    ui::LXXKnob midPitch_ { "PITCH" };
    ui::LXXKnob midDecay_ { "DECAY" };
    ui::LXXKnob midTone_ { "TONE" };
    ui::LXXKnob volKick_ { "VOL" };
    ui::LXXKnob volSnare_ { "VOL" };
    ui::LXXKnob volCh_ { "VOL" };
    ui::LXXKnob volLow_ { "VOL" };
    ui::LXXKnob volMid_ { "VOL" };
    ui::LXXKnob volOh_ { "VOL" };
    ui::LXXKnob swing_ { "SWING" };
    ui::LXXKnob tempo_ { "TEMPO" };
    ui::LXXKnob volume_ { "MASTER", ui::KnobSize::large };
    ui::SeqGridComponent seqGrid_;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> hostSyncAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> midiOutAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> runAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> seqOnlyAttachment_;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> muteLockAttachments_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> clockDivisionAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> drumPageAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sequencerModeAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> uiPageAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> guiSubAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lowCharacterAttachment_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LXXAudioProcessorEditor)
};

}  // namespace lxx
