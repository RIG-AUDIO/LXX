#include "parameters/ParameterLayout.h"

#include "parameters/ParameterIDs.h"

namespace lxx {

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace params;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::volume, "Volume", 0.0f, 1.0f, 0.8f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::tempo, "Tempo",
        juce::NormalisableRange<float>(40.0f, 240.0f, 1.0f), 120.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        IDs::hostSync, "Host Sync", true));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::clockDivision, "Clock Div",
        juce::StringArray { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" }, 4));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::swing, "Swing", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        IDs::seqRun, "Run", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::drumsX, "Drums X", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::drumsY, "Drums Y", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::drumsBalance, "Drums Balance", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::drumsBandwidth, "Bitcrush / Bandwidth", 0.0f, 1.0f, 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::bdDensity, "BD Density", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::sdDensity, "SD Density", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::hhDensity, "HH Density", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::bdTone, "BD Tone", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::sdTone, "SD Tone", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::hhTone, "HH Tone", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::drumPage, "Drum Page",
        juce::StringArray { "Simple", "Extended" }, 0));

    layout.add(std::make_unique<juce::AudioParameterInt>(
        IDs::bdPitch, "BD Pitch", 0, 255, 60));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        IDs::bdPitchDecay, "BD Pitch Decay", 0, 255, 42));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        IDs::bdPitchMod, "BD Pitch Env", 0, 255, 130));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        IDs::bdAmpDecay, "BD Decay", 0, 255, 180));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        IDs::bdCrunch, "BD Crunch", 0, 255, 14));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        IDs::sdPitch, "SD Pitch", 0, 255, 108));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        IDs::sdPitchDecay, "SD Pitch Decay", 0, 255, 36));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        IDs::sdPitchMod, "SD Pitch Env", 0, 255, 50));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        IDs::sdAmpDecay, "SD Decay", 0, 255, 90));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        IDs::sdSnappy, "SD Snappy", 0, 255, 180));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        IDs::hhPitch, "HH Pitch", 0, 255, 136));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        IDs::hhAmpDecay, "HH Decay", 0, 255, 110));

    // Compatibility: old states may store index 1 (Analog) — runtime forces native.
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::drumEngine, "Drum Engine",
        juce::StringArray { "Native", "Native" }, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::sequencerMode, "Sequencer",
        juce::StringArray { "Legacy", "Grids", "Techno", "Tech Ext", "DNB", "DNB Ext" }, 0));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        IDs::midiOut, "MIDI Out", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        IDs::seqOnly, "Seq Only", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::uiPage, "UI Page",
        juce::StringArray { "GUI", "GUI+CLI", "CLI" }, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::guiSubPage, "GUI Sub",
        juce::StringArray { "Main", "Advanced" }, 0));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::gridsChaos, "Grids Chaos", 0.0f, 1.0f, 0.15f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::technoDensity, "Techno Density", 0.0f, 1.0f, 0.45f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::technoVariation, "Techno Variation", 0.0f, 1.0f, 0.25f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::technoSyncopation, "Techno Syncopation", 0.0f, 1.0f, 0.2f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::technoFill, "Techno Fill", 0.0f, 1.0f, 0.35f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::technoSwing, "Techno Swing", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        IDs::technoSeed, "Techno Seed", 1, 9999, 1));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::obsoleteKickAttackFm, "Legacy Atk FM", 0.0f, 1.0f, 0.35f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::obsoleteKickSelfFm, "Legacy Self FM", 0.0f, 1.0f, 0.2f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::obsoleteHatNoisiness, "Legacy Hat Noise", 0.0f, 1.0f, 0.45f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::ohDecay, "Open Hat Decay", 0.0f, 1.0f, 0.75f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::ohTone, "Open Hat Tone", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::lowPitch, "Low Perc Pitch", 0.0f, 1.0f, 0.35f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::lowDecay, "Low Perc Decay", 0.0f, 1.0f, 0.55f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::lowTone, "Low Perc Tone", 0.0f, 1.0f, 0.35f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::midPitch, "Mid Perc Pitch", 0.0f, 1.0f, 0.45f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::midDecay, "Mid Perc Decay", 0.0f, 1.0f, 0.4f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::midTone, "Mid Perc Tone", 0.0f, 1.0f, 0.55f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::lowCharacter, "Low Character",
        juce::StringArray { "Auto", "Boom", "Tom" }, 0));

    auto addVol = [&](const char* id, const char* name) {
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            id, name,
            juce::NormalisableRange<float>(-60.0f, 6.0f, 0.1f, 0.4f), 0.0f));
    };
    addVol(IDs::volKick, "Kick Vol");
    addVol(IDs::volSnare, "Snare Vol");
    addVol(IDs::volCh, "CH Vol");
    addVol(IDs::volLow, "Boom Vol");
    addVol(IDs::volMid, "Perc Vol");
    addVol(IDs::volOh, "OH Vol");

    layout.add(std::make_unique<juce::AudioParameterBool>(IDs::muteKick, "Mute Kick", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(IDs::muteSnare, "Mute Snare", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(IDs::muteCh, "Mute CH", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(IDs::muteLow, "Mute Boom", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(IDs::muteMid, "Mute Perc", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(IDs::muteOh, "Mute OH", false));

    layout.add(std::make_unique<juce::AudioParameterBool>(IDs::lockKick, "Lock Kick", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(IDs::lockSnare, "Lock Snare", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(IDs::lockCh, "Lock CH", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(IDs::lockLow, "Lock Boom", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(IDs::lockMid, "Lock Perc", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(IDs::lockOh, "Lock OH", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(IDs::lockTimbre, "Lock Timbre", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(IDs::lockSeq, "Lock Seq", false));

    return layout;
}

}  // namespace lxx
