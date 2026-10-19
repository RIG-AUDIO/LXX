#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <random>
#include <vector>

namespace lxx::ui {

struct CommandSpec {
    const char* name = nullptr;
    const char* help = nullptr;
    const char* const* args = nullptr;
};

/** Single registry driving help, parse, and autocomplete. */
class CommandRegistry {
public:
    static const std::vector<CommandSpec>& all();
    static juce::StringArray completionsFor(const juce::StringArray& tokens, bool endsWithSpace);
};

/**
 * Command router. Randomization locks live in APVTS (shared with GUI).
 */
class TerminalCommandRouter {
public:
    bool tryExecute(const juce::String& line,
                    juce::AudioProcessorValueTreeState& apvts,
                    juce::String& feedback);

    juce::StringArray complete(const juce::String& line, bool endsWithSpace) const
    {
        juce::StringArray toks;
        toks.addTokens(line.trimStart(), " \t", "\"'");
        toks.removeEmptyStrings();
        return CommandRegistry::completionsFor(toks, endsWithSpace || line.endsWithChar(' '));
    }

private:
    std::mt19937 rng_ { 0xA712u };

    bool setBool(juce::AudioProcessorValueTreeState& apvts, const char* id, bool on);
    bool setChoiceIndex(juce::AudioProcessorValueTreeState& apvts, const char* id, int index);
    bool setFloat01(juce::AudioProcessorValueTreeState& apvts, const char* id, float v01);
    bool setFloatDb(juce::AudioProcessorValueTreeState& apvts, const char* id, float db);
    bool setNormPercent(juce::AudioProcessorValueTreeState& apvts, const char* id, float pct);
    bool setIntParam(juce::AudioProcessorValueTreeState& apvts, const char* id, int value);
    float getRaw(juce::AudioProcessorValueTreeState& apvts, const char* id) const;
    float getFloatDb(juce::AudioProcessorValueTreeState& apvts, const char* id) const;
    int getChoiceIndex(juce::AudioProcessorValueTreeState& apvts, const char* id) const;
    bool getBool(juce::AudioProcessorValueTreeState& apvts, const char* id) const;
    float rand01();
    float nudge(float cur, float lo, float hi, float amount01);
    void mutateSeed(juce::AudioProcessorValueTreeState& apvts, int delta, juce::String& feedback);
    void applyTechnoPreset(juce::AudioProcessorValueTreeState& apvts, const juce::String& name, juce::String& feedback);
    void applyDnbPreset(juce::AudioProcessorValueTreeState& apvts, const juce::String& name, juce::String& feedback);
    void randomizeSeq(juce::AudioProcessorValueTreeState& apvts, float amount01, juce::String& feedback);
    void randomizeTimbre(juce::AudioProcessorValueTreeState& apvts, float amount01, juce::String& feedback);
    bool setLockVoice(juce::AudioProcessorValueTreeState& apvts, const juce::String& voice, bool on, juce::String& feedback);
    juce::String mixerReport(juce::AudioProcessorValueTreeState& apvts) const;
};

}  // namespace lxx::ui
