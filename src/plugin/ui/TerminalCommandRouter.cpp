#include "ui/TerminalCommandRouter.h"
#include "parameters/ParameterIDs.h"
#include <cmath>

namespace lxx::ui {

namespace {
juce::StringArray tokenize(const juce::String& line)
{
    juce::StringArray toks;
    toks.addTokens(line.trim(), " \t", "\"'");
    toks.removeEmptyStrings();
    return toks;
}
float clamp01(float v) { return juce::jlimit(0.0f, 1.0f, v); }

static const char* kSeqArgs[] = { "legacy", "grids", "techno", "tech-ext", "extended", "ext", "dnb", "dnb-ext", nullptr };
static const char* kOnOff[] = { "on", "off", nullptr };
static const char* kRandArgs[] = { "seq", "timbre", "kit", "all", nullptr };
static const char* kLockArgs[] = {
    "kick", "snare", "ch", "hats", "boom", "low", "perc", "mid", "oh", "timbre", "seq", "all", nullptr
};
static const char* kVoiceArgs[] = {
    "kick", "snare", "ch", "hat", "hh", "boom", "low", "perc", "mid", "oh", nullptr
};
static const char* kTechnoArgs[] = { "minimal", "hypnotic", "driving", "peak", "raw", nullptr };
static const char* kDnbArgs[] = { "clean", "rolling", "deep", "jungle", "neuro", nullptr };
static const char* kGenArgs[] = { "techno", "extended", "ext", "dnb", "dnb-ext", nullptr };
static const char* kLowCharArgs[] = { "auto", "boom", "tom", "character", nullptr };

const char* volIdForVoice(const juce::String& a)
{
    using namespace lxx::params;
    if (a == "kick" || a == "bd") return IDs::volKick;
    if (a == "snare" || a == "sd") return IDs::volSnare;
    if (a == "ch" || a == "hat" || a == "hh") return IDs::volCh;
    if (a == "boom" || a == "low") return IDs::volLow;
    if (a == "perc" || a == "mid") return IDs::volMid;
    if (a == "oh") return IDs::volOh;
    return nullptr;
}
const char* muteIdForVoice(const juce::String& a)
{
    using namespace lxx::params;
    if (a == "kick" || a == "bd") return IDs::muteKick;
    if (a == "snare" || a == "sd") return IDs::muteSnare;
    if (a == "ch" || a == "hat" || a == "hh") return IDs::muteCh;
    if (a == "boom" || a == "low") return IDs::muteLow;
    if (a == "perc" || a == "mid") return IDs::muteMid;
    if (a == "oh") return IDs::muteOh;
    return nullptr;
}
}  // namespace

const std::vector<CommandSpec>& CommandRegistry::all()
{
    static const std::vector<CommandSpec> k {
        { "help", "list commands", nullptr },
        { "status", "show generator state", nullptr },
        { "mixer", "six-voice levels/mutes/locks", nullptr },
        { "run", "start sequencer", nullptr },
        { "stop", "stop sequencer", nullptr },
        { "panic", "stop + clear notes", nullptr },
        { "seq", "set sequencer mode", kSeqArgs },
        { "audio", "on|off (off=seqonly)", kOnOff },
        { "midi", "on|off", kOnOff },
        { "seqonly", "on|off", kOnOff },
        { "generate", "new seed / optional genre", kGenArgs },
        { "regen", "alias generate", kGenArgs },
        { "seed", "N|random", nullptr },
        { "mutate", "soft pattern evolve [0-100]", nullptr },
        { "density", "0-100", nullptr },
        { "variation", "0-100", nullptr },
        { "syncopation", "0-100", nullptr },
        { "fill", "0-100", nullptr },
        { "swing", "0-100", nullptr },
        { "randomize", "seq|timbre|kit|all [amt]", kRandArgs },
        { "lock", "voice|timbre|seq|all", kLockArgs },
        { "unlock", "voice|timbre|seq|all", kLockArgs },
        { "mute", "voice", kVoiceArgs },
        { "unmute", "voice", kVoiceArgs },
        { "volume", "voice dB", kVoiceArgs },
        { "master", "0-100 | -inf..+6 dB", nullptr },
        { "bitcrush", "0-100 (0=crush, 100=open)", nullptr },
        { "bandwidth", "alias of bitcrush", nullptr },
        { "techno", "minimal|hypnotic|driving|peak|raw", kTechnoArgs },
        { "dnb", "clean|rolling|deep|jungle|neuro", kDnbArgs },
        { "lane", "kick|snare|ch density N", nullptr },
        { "low", "character auto|boom|tom", kLowCharArgs },
    };
    return k;
}

juce::StringArray CommandRegistry::completionsFor(const juce::StringArray& tokens, bool endsWithSpace)
{
    juce::StringArray out;
    const auto& specs = all();
    if (tokens.isEmpty() || (tokens.size() == 1 && !endsWithSpace)) {
        const auto prefix = tokens.isEmpty() ? juce::String() : tokens[0].toLowerCase();
        for (auto& s : specs)
            if (prefix.isEmpty() || juce::String(s.name).startsWith(prefix))
                out.add(s.name);
        return out;
    }

    const auto cmd = tokens[0].toLowerCase();
    const CommandSpec* spec = nullptr;
    for (auto& s : specs)
        if (cmd == s.name) { spec = &s; break; }
    if (spec == nullptr || spec->args == nullptr)
        return out;

    juce::String prefix;
    if (!endsWithSpace && tokens.size() >= 2)
        prefix = tokens[tokens.size() - 1].toLowerCase();

    for (auto* a = spec->args; *a != nullptr; ++a)
        if (prefix.isEmpty() || juce::String(*a).startsWith(prefix))
            out.add(*a);
    return out;
}

float TerminalCommandRouter::rand01()
{
    return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng_);
}
float TerminalCommandRouter::nudge(float cur, float lo, float hi, float amount01)
{
    const float span = (hi - lo) * amount01;
    return juce::jlimit(lo, hi, cur + (rand01() * 2.0f - 1.0f) * span);
}
bool TerminalCommandRouter::setBool(juce::AudioProcessorValueTreeState& apvts, const char* id, bool on)
{
    if (auto* p = apvts.getParameter(id)) {
        p->beginChangeGesture();
        p->setValueNotifyingHost(on ? 1.0f : 0.0f);
        p->endChangeGesture();
        return true;
    }
    return false;
}
bool TerminalCommandRouter::setChoiceIndex(juce::AudioProcessorValueTreeState& apvts, const char* id, int index)
{
    auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(id));
    if (p == nullptr) return false;
    const float v = p->convertTo0to1(static_cast<float>(juce::jlimit(0, p->choices.size() - 1, index)));
    p->beginChangeGesture();
    p->setValueNotifyingHost(v);
    p->endChangeGesture();
    return true;
}
bool TerminalCommandRouter::setFloat01(juce::AudioProcessorValueTreeState& apvts, const char* id, float v01)
{
    if (auto* p = apvts.getParameter(id)) {
        p->beginChangeGesture();
        p->setValueNotifyingHost(clamp01(v01));
        p->endChangeGesture();
        return true;
    }
    return false;
}
bool TerminalCommandRouter::setFloatDb(juce::AudioProcessorValueTreeState& apvts, const char* id, float db)
{
    auto* p = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(id));
    if (p == nullptr) return false;
    db = juce::jlimit(-60.0f, 6.0f, db);
    p->beginChangeGesture();
    p->setValueNotifyingHost(p->convertTo0to1(db));
    p->endChangeGesture();
    return true;
}
bool TerminalCommandRouter::setNormPercent(juce::AudioProcessorValueTreeState& apvts, const char* id, float pct)
{
    return setFloat01(apvts, id, juce::jlimit(0.0f, 100.0f, pct) * 0.01f);
}
bool TerminalCommandRouter::setIntParam(juce::AudioProcessorValueTreeState& apvts, const char* id, int value)
{
    auto* p = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id));
    if (p == nullptr) return false;
    p->beginChangeGesture();
    p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(value)));
    p->endChangeGesture();
    return true;
}
float TerminalCommandRouter::getRaw(juce::AudioProcessorValueTreeState& apvts, const char* id) const
{
    if (auto* v = apvts.getRawParameterValue(id)) return v->load();
    return 0.0f;
}
float TerminalCommandRouter::getFloatDb(juce::AudioProcessorValueTreeState& apvts, const char* id) const
{
    if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(id)))
        return p->get();
    return 0.0f;
}
int TerminalCommandRouter::getChoiceIndex(juce::AudioProcessorValueTreeState& apvts, const char* id) const
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(id)))
        return p->getIndex();
    return 0;
}
bool TerminalCommandRouter::getBool(juce::AudioProcessorValueTreeState& apvts, const char* id) const
{
    return getRaw(apvts, id) >= 0.5f;
}

void TerminalCommandRouter::mutateSeed(juce::AudioProcessorValueTreeState& apvts, int delta, juce::String& feedback)
{
    using namespace params;
    if (getBool(apvts, IDs::lockSeq)) { feedback = "SEQ LOCKED"; return; }
    auto* p = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter(IDs::technoSeed));
    int seed = p != nullptr ? p->get() : 1;
    seed = juce::jlimit(1, 9999, seed + delta);
    setIntParam(apvts, IDs::technoSeed, seed);
    feedback = "PATTERN MUTATED / SEED " + juce::String(seed);
}

void TerminalCommandRouter::applyTechnoPreset(juce::AudioProcessorValueTreeState& apvts,
                                              const juce::String& name, juce::String& feedback)
{
    using namespace params;
    setChoiceIndex(apvts, IDs::sequencerMode, 2);
    float dens = 0.28f, var = 0.12f, sync = 0.08f, fill = 0.1f, swing = 0.0f;
    if (name == "hypnotic") { dens = 0.58f; var = 0.18f; sync = 0.14f; fill = 0.22f; swing = 0.08f; }
    else if (name == "driving") { dens = 0.72f; var = 0.22f; sync = 0.2f; fill = 0.28f; }
    else if (name == "peak") { dens = 0.88f; var = 0.35f; sync = 0.3f; fill = 0.55f; swing = 0.05f; }
    else if (name == "raw") { dens = 0.65f; var = 0.45f; sync = 0.4f; fill = 0.35f; swing = 0.12f; }
    else if (name != "minimal") { feedback = "UNKNOWN TECHNO PRESET"; return; }
    if (!getBool(apvts, IDs::lockSeq)) {
        setFloat01(apvts, IDs::technoDensity, dens);
        setFloat01(apvts, IDs::technoVariation, var);
        setFloat01(apvts, IDs::technoSyncopation, sync);
        setFloat01(apvts, IDs::technoFill, fill);
        setFloat01(apvts, IDs::technoSwing, swing);
        const int seed = 1000 + static_cast<int>(rand01() * 8000.0f);
        setIntParam(apvts, IDs::technoSeed, seed);
        feedback = "MODE TECHNO\nDENSITY " + juce::String(static_cast<int>(dens * 100))
            + "\nVAR " + juce::String(static_cast<int>(var * 100))
            + "\nSEED " + juce::String(seed);
    } else feedback = "MODE TECHNO / SEQ LOCKED";
}

void TerminalCommandRouter::applyDnbPreset(juce::AudioProcessorValueTreeState& apvts,
                                           const juce::String& name, juce::String& feedback)
{
    using namespace params;
    setChoiceIndex(apvts, IDs::sequencerMode, 4);
    float dens = 0.45f, var = 0.2f, sync = 0.35f, fill = 0.25f, swing = 0.08f;
    juce::String label = "CLEAN";
    if (name == "rolling") { dens = 0.62f; var = 0.28f; sync = 0.45f; fill = 0.3f; swing = 0.12f; label = "ROLLING"; }
    else if (name == "deep") { dens = 0.4f; var = 0.18f; sync = 0.3f; fill = 0.2f; swing = 0.05f; label = "DEEP"; }
    else if (name == "jungle") { dens = 0.7f; var = 0.4f; sync = 0.55f; fill = 0.45f; swing = 0.15f; label = "JUNGLE"; }
    else if (name == "neuro") { dens = 0.68f; var = 0.35f; sync = 0.5f; fill = 0.35f; swing = 0.1f; label = "NEURO"; }
    else if (name != "clean") { feedback = "UNKNOWN DNB PRESET"; return; }
    if (!getBool(apvts, IDs::lockSeq)) {
        setFloat01(apvts, IDs::technoDensity, dens);
        setFloat01(apvts, IDs::technoVariation, var);
        setFloat01(apvts, IDs::technoSyncopation, sync);
        setFloat01(apvts, IDs::technoFill, fill);
        setFloat01(apvts, IDs::technoSwing, swing);
        const int seed = 1000 + static_cast<int>(rand01() * 8000.0f);
        setIntParam(apvts, IDs::technoSeed, seed);
        feedback = "PROFILE " + label + " / SEED " + juce::String(seed);
    } else feedback = "DNB / SEQ LOCKED";
}

void TerminalCommandRouter::randomizeSeq(juce::AudioProcessorValueTreeState& apvts, float amount01, juce::String& feedback)
{
    using namespace params;
    if (getBool(apvts, IDs::lockSeq)) { feedback = "SEQ LOCKED"; return; }
    amount01 = clamp01(amount01);
    const int mode = getChoiceIndex(apvts, IDs::sequencerMode);
    auto nudgeP = [&](const char* id, float lo, float hi) {
        setFloat01(apvts, id, nudge(getRaw(apvts, id), lo, hi, amount01));
    };
    if (mode >= 4) {
        nudgeP(IDs::technoDensity, 0.25f, 0.9f);
        nudgeP(IDs::technoSyncopation, 0.25f, 0.85f);
        nudgeP(IDs::technoVariation, 0.1f, 0.7f);
    } else {
        nudgeP(IDs::technoDensity, 0.15f, 0.9f);
        nudgeP(IDs::technoVariation, 0.05f, 0.7f);
        nudgeP(IDs::technoSyncopation, 0.05f, 0.6f);
    }
    nudgeP(IDs::technoFill, 0.05f, 0.85f);
    if (amount01 > 0.35f)
        nudgeP(IDs::technoSwing, 0.0f, 0.35f);
    auto* p = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter(IDs::technoSeed));
    int seed = p != nullptr ? p->get() : 1;
    seed = juce::jlimit(1, 9999, seed + static_cast<int>((rand01() * 2.0f - 1.0f) * (amount01 * 200.0f + 1.0f)));
    setIntParam(apvts, IDs::technoSeed, seed);
    feedback = "SEQ MUTATED / SEED " + juce::String(seed);
}

void TerminalCommandRouter::randomizeTimbre(juce::AudioProcessorValueTreeState& apvts, float amount01, juce::String& feedback)
{
    using namespace params;
    if (getBool(apvts, IDs::lockTimbre)) { feedback = "TIMBRE LOCKED"; return; }
    amount01 = clamp01(amount01);
    const bool lk = getBool(apvts, IDs::lockKick);
    const bool ls = getBool(apvts, IDs::lockSnare);
    const bool lc = getBool(apvts, IDs::lockCh);
    const bool ll = getBool(apvts, IDs::lockLow);
    const bool lm = getBool(apvts, IDs::lockMid);
    const bool lo = getBool(apvts, IDs::lockOh);

    auto nudgeF = [&](const char* id, float loB, float hi, bool locked) {
        if (!locked) setFloat01(apvts, id, nudge(getRaw(apvts, id), loB, hi, amount01));
    };
    auto nudgeI = [&](const char* id, int loB, int hi, bool locked) {
        if (locked) return;
        auto* p = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter(id));
        if (p == nullptr) return;
        const float mapped = static_cast<float>(loB) + getRaw(apvts, id) * static_cast<float>(hi - loB);
        setIntParam(apvts, id, static_cast<int>(std::lround(nudge(mapped, (float) loB, (float) hi, amount01))));
    };
    nudgeI(IDs::bdPitch, 40, 100, lk);
    nudgeI(IDs::bdAmpDecay, 80, 220, lk);
    nudgeF(IDs::bdTone, 0.2f, 0.8f, lk);
    nudgeI(IDs::bdCrunch, 0, 80, lk);
    nudgeI(IDs::sdPitch, 70, 180, ls);
    nudgeI(IDs::sdAmpDecay, 50, 200, ls);
    nudgeI(IDs::sdSnappy, 50, 210, ls);
    nudgeF(IDs::sdTone, 0.2f, 0.8f, ls);
    nudgeI(IDs::hhPitch, 90, 200, lc);
    nudgeI(IDs::hhAmpDecay, 30, 160, lc);
    nudgeF(IDs::hhTone, 0.2f, 0.8f, lc);
    nudgeF(IDs::ohDecay, 0.45f, 0.95f, lo);
    nudgeF(IDs::ohTone, 0.2f, 0.8f, lo);
    nudgeF(IDs::lowPitch, 0.15f, 0.7f, ll);
    nudgeF(IDs::lowDecay, 0.25f, 0.85f, ll);
    nudgeF(IDs::lowTone, 0.15f, 0.7f, ll);
    nudgeF(IDs::midPitch, 0.2f, 0.75f, lm);
    nudgeF(IDs::midDecay, 0.15f, 0.7f, lm);
    nudgeF(IDs::midTone, 0.2f, 0.8f, lm);

    juce::String extra;
    if (lk) extra += " / KICK LOCKED";
    if (ls) extra += " / SNARE LOCKED";
    if (lc) extra += " / CH LOCKED";
    if (ll) extra += " / BOOM LOCKED";
    if (lm) extra += " / PERC LOCKED";
    if (lo) extra += " / OH LOCKED";
    feedback = "NATIVE KIT MUTATED" + extra;
}

bool TerminalCommandRouter::setLockVoice(juce::AudioProcessorValueTreeState& apvts,
                                         const juce::String& voice, bool on, juce::String& feedback)
{
    using namespace params;
    const auto a = voice.toLowerCase();
    if (a == "kick" || a == "bd") { setBool(apvts, IDs::lockKick, on); feedback = (on ? "LOCKED " : "UNLOCKED ") + juce::String("KICK"); return true; }
    if (a == "snare" || a == "sd") { setBool(apvts, IDs::lockSnare, on); feedback = (on ? "LOCKED " : "UNLOCKED ") + juce::String("SNARE"); return true; }
    if (a == "ch" || a == "hat" || a == "hh") { setBool(apvts, IDs::lockCh, on); feedback = (on ? "LOCKED " : "UNLOCKED ") + juce::String("CH"); return true; }
    if (a == "hats") {
        setBool(apvts, IDs::lockCh, on);
        setBool(apvts, IDs::lockOh, on);
        feedback = (on ? "LOCKED " : "UNLOCKED ") + juce::String("HATS");
        return true;
    }
    if (a == "boom" || a == "low") { setBool(apvts, IDs::lockLow, on); feedback = (on ? "LOCKED " : "UNLOCKED ") + juce::String("BOOM"); return true; }
    if (a == "perc" || a == "mid") { setBool(apvts, IDs::lockMid, on); feedback = (on ? "LOCKED " : "UNLOCKED ") + juce::String("PERC"); return true; }
    if (a == "oh") { setBool(apvts, IDs::lockOh, on); feedback = (on ? "LOCKED " : "UNLOCKED ") + juce::String("OH"); return true; }
    if (a == "timbre") { setBool(apvts, IDs::lockTimbre, on); feedback = (on ? "LOCKED " : "UNLOCKED ") + juce::String("TIMBRE"); return true; }
    if (a == "seq") { setBool(apvts, IDs::lockSeq, on); feedback = (on ? "LOCKED " : "UNLOCKED ") + juce::String("SEQ"); return true; }
    if (a == "all") {
        for (auto* id : { IDs::lockKick, IDs::lockSnare, IDs::lockCh, IDs::lockLow, IDs::lockMid, IDs::lockOh, IDs::lockTimbre, IDs::lockSeq })
            setBool(apvts, id, on);
        feedback = on ? "LOCKED ALL" : "UNLOCKED ALL";
        return true;
    }
    feedback = "USAGE: lock|unlock kick|snare|ch|boom|perc|oh|hats|timbre|seq|all";
    return false;
}

juce::String TerminalCommandRouter::mixerReport(juce::AudioProcessorValueTreeState& apvts) const
{
    using namespace params;
    auto line = [&](const char* name, const char* vol, const char* mute, const char* lock) {
        const float db = getFloatDb(apvts, vol);
        const bool m = getBool(apvts, mute);
        const bool l = getBool(apvts, lock);
        juce::String s = juce::String(name).paddedRight(' ', 8)
            + juce::String(db, 1).paddedLeft(' ', 6) + " dB  "
            + (m ? "MUTED" : "ON   ");
        if (l) s += "  LOCK";
        return s;
    };
    return line("KICK", IDs::volKick, IDs::muteKick, IDs::lockKick) + "\n"
        + line("SNARE", IDs::volSnare, IDs::muteSnare, IDs::lockSnare) + "\n"
        + line("CH", IDs::volCh, IDs::muteCh, IDs::lockCh) + "\n"
        + line("BOOM", IDs::volLow, IDs::muteLow, IDs::lockLow) + "\n"
        + line("PERC", IDs::volMid, IDs::muteMid, IDs::lockMid) + "\n"
        + line("OH", IDs::volOh, IDs::muteOh, IDs::lockOh);
}

bool TerminalCommandRouter::tryExecute(const juce::String& lineIn,
                                       juce::AudioProcessorValueTreeState& apvts,
                                       juce::String& feedback)
{
    using namespace params;
    const auto toks = tokenize(lineIn);
    if (toks.isEmpty()) { feedback = {}; return true; }
    const auto cmd = toks[0].toLowerCase();

    if (cmd == "help") {
        feedback = "run stop panic status mixer | seq … | volume voice dB | mute|unmute voice\n"
                   "master 0-100 | bitcrush 0-100 | lock|unlock kick|snare|ch|boom|perc|oh|hats|timbre|seq|all\n"
                   "generate mutate randomize seq|timbre|kit|all | low character auto|boom|tom";
        return true;
    }
    if (cmd == "mixer") { feedback = mixerReport(apvts); return true; }
    if (cmd == "status") {
        static const char* modes[] = { "LEGACY", "GRIDS", "TECHNO", "TECHNO EXT", "DNB", "DNB EXT" };
        static const char* lowModes[] = { "AUTO", "BOOM", "TOM" };
        const int mode = juce::jlimit(0, 5, getChoiceIndex(apvts, IDs::sequencerMode));
        const int low = juce::jlimit(0, 2, getChoiceIndex(apvts, IDs::lowCharacter));
        auto* seedP = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter(IDs::technoSeed));
        feedback = juce::String("MODE       ") + modes[mode]
            + "\nENGINE     NATIVE"
            + "\nRUN        " + juce::String(getBool(apvts, IDs::seqRun) ? "ON" : "OFF")
            + "\nMIDI       " + juce::String(getBool(apvts, IDs::midiOut) ? "ON" : "OFF")
            + "\nSEED       " + juce::String(seedP ? seedP->get() : 0)
            + "\nDENSITY    " + juce::String(static_cast<int>(getRaw(apvts, IDs::technoDensity) * 100))
            + "\nLOW CHAR   " + lowModes[low]
            + "\n(use MIXER for levels)";
        return true;
    }
    if (cmd == "run") { setBool(apvts, IDs::seqRun, true); feedback = "RUN ON"; return true; }
    if (cmd == "stop") { setBool(apvts, IDs::seqRun, false); feedback = "RUN OFF"; return true; }
    if (cmd == "panic") { setBool(apvts, IDs::seqRun, false); feedback = "PANIC / NOTES CLEARED"; return true; }

    if (cmd == "seq" && toks.size() >= 2) {
        const auto a = toks[1].toLowerCase();
        int idx = -1;
        if (a == "legacy") idx = 0;
        else if (a == "grids") idx = 1;
        else if (a == "techno") idx = 2;
        else if (a == "extended" || a == "ext" || a == "tech-ext" || a == "technoext") idx = 3;
        else if (a == "dnb") idx = 4;
        else if (a == "dnb-ext" || a == "dnbext") idx = 5;
        if (idx < 0) { feedback = "USAGE: seq legacy|grids|techno|tech-ext|dnb|dnb-ext"; return false; }
        setChoiceIndex(apvts, IDs::sequencerMode, idx);
        static const char* msg[] = { "MODE LEGACY", "MODE GRIDS", "MODE TECHNO", "TECHNO EXT / 6 LANES", "MODE DNB", "DNB EXT / 6 LANES" };
        feedback = msg[idx];
        return true;
    }
    if (cmd == "engine") { feedback = "ENGINE FIXED NATIVE (analog removed)"; return true; }
    if (cmd == "low") {
        juce::String a;
        if (toks.size() >= 3 && toks[1].equalsIgnoreCase("character"))
            a = toks[2].toLowerCase();
        else if (toks.size() >= 2)
            a = toks[1].toLowerCase();
        else { feedback = "USAGE: low character auto|boom|tom"; return false; }
        int idx = -1;
        if (a == "auto") idx = 0;
        else if (a == "boom") idx = 1;
        else if (a == "tom") idx = 2;
        if (idx < 0) { feedback = "USAGE: low character auto|boom|tom"; return false; }
        setChoiceIndex(apvts, IDs::lowCharacter, idx);
        static const char* msg[] = { "LOW ARTICULATION AUTO", "LOW ARTICULATION BOOM", "LOW ARTICULATION TOM" };
        feedback = msg[idx];
        return true;
    }
    if (cmd == "volume" && toks.size() >= 3) {
        const auto* id = volIdForVoice(toks[1].toLowerCase());
        if (id == nullptr) { feedback = "USAGE: volume kick|snare|ch|boom|perc|oh <dB>"; return false; }
        const float db = (float) toks[2].getDoubleValue();
        setFloatDb(apvts, id, db);
        feedback = toks[1].toUpperCase() + " " + juce::String(juce::jlimit(-60.0f, 6.0f, db), 1) + " dB";
        return true;
    }
    if ((cmd == "master" || cmd == "vol") && toks.size() >= 2) {
        float pct = (float) toks[1].getDoubleValue();
        // Accept 0..1 or 0..100
        if (pct > 1.0f) pct *= 0.01f;
        setFloat01(apvts, IDs::volume, pct);
        feedback = "MASTER " + juce::String(static_cast<int>(std::lround(clamp01(pct) * 100.0f)));
        return true;
    }
    if ((cmd == "bitcrush" || cmd == "bandwidth") && toks.size() >= 2) {
        float pct = (float) toks[1].getDoubleValue();
        if (pct > 1.0f) pct *= 0.01f;
        // APVTS drums_bandwidth: 1 = open (least crush), 0 = max crush
        setFloat01(apvts, IDs::drumsBandwidth, pct);
        feedback = "BITCRUSH/BANDWIDTH " + juce::String(static_cast<int>(std::lround(clamp01(pct) * 100.0f)))
            + (clamp01(pct) >= 0.99f ? " (OPEN)" : "");
        return true;
    }
    if ((cmd == "mute" || cmd == "unmute") && toks.size() >= 2) {
        const auto* id = muteIdForVoice(toks[1].toLowerCase());
        if (id == nullptr) { feedback = "USAGE: mute|unmute kick|snare|ch|boom|perc|oh"; return false; }
        setBool(apvts, id, cmd == "mute");
        feedback = toks[1].toUpperCase() + (cmd == "mute" ? " MUTED" : " UNMUTED");
        return true;
    }
    if (cmd == "audio" && toks.size() >= 2) {
        const bool on = toks[1].equalsIgnoreCase("on");
        setBool(apvts, IDs::seqOnly, !on);
        if (!on) setBool(apvts, IDs::midiOut, true);
        feedback = on ? "AUDIO ON" : "MIDI ONLY / DSP BYPASSED";
        return true;
    }
    if (cmd == "midi" && toks.size() >= 2) {
        const bool on = toks[1].equalsIgnoreCase("on");
        if (!on && getBool(apvts, IDs::seqOnly)) { feedback = "SEQ ONLY REQUIRES MIDI"; return false; }
        setBool(apvts, IDs::midiOut, on);
        feedback = on ? "MIDI ON" : "MIDI OFF";
        return true;
    }
    if (cmd == "seqonly" && toks.size() >= 2) {
        const bool on = toks[1].equalsIgnoreCase("on");
        setBool(apvts, IDs::seqOnly, on);
        if (on) setBool(apvts, IDs::midiOut, true);
        feedback = on ? "MIDI ONLY / DSP BYPASSED" : "SEQ ONLY OFF";
        return true;
    }
    if (cmd == "generate" || cmd == "regen") {
        if (getBool(apvts, IDs::lockSeq)) { feedback = "SEQ LOCKED"; return true; }
        juce::String hint;
        if (toks.size() >= 2) {
            const auto a = toks[1].toLowerCase();
            if (a == "techno") { setChoiceIndex(apvts, IDs::sequencerMode, 2); hint = "TECHNO "; }
            else if (a == "extended" || a == "ext" || a == "tech-ext") { setChoiceIndex(apvts, IDs::sequencerMode, 3); hint = "TECHNO EXT "; }
            else if (a == "dnb") { setChoiceIndex(apvts, IDs::sequencerMode, 4); hint = "DNB "; }
            else if (a == "dnb-ext") { setChoiceIndex(apvts, IDs::sequencerMode, 5); hint = "DNB EXT "; }
        }
        const int seed = 1 + static_cast<int>(rand01() * 9998.0f);
        setIntParam(apvts, IDs::technoSeed, seed);
        feedback = hint + "GENERATED / SEED " + juce::String(seed);
        return true;
    }
    if (cmd == "seed" && toks.size() >= 2) {
        if (getBool(apvts, IDs::lockSeq)) { feedback = "SEQ LOCKED"; return true; }
        int seed = toks[1].equalsIgnoreCase("random") ? (1 + (int) (rand01() * 9998.0f))
                                                       : juce::jlimit(1, 9999, toks[1].getIntValue());
        setIntParam(apvts, IDs::technoSeed, seed);
        feedback = "SEED " + juce::String(seed);
        return true;
    }
    if (cmd == "mutate") {
        const int amt = toks.size() >= 2 ? juce::jlimit(1, 100, toks[1].getIntValue()) : 20;
        mutateSeed(apvts, juce::jmax(1, amt / 2), feedback);
        if (!getBool(apvts, IDs::lockSeq) && amt > 15)
            setFloat01(apvts, IDs::technoVariation,
                       nudge(getRaw(apvts, IDs::technoVariation), 0.05f, 0.9f, amt * 0.0035f));
        return true;
    }
    auto macro = [&](const char* id, const juce::String& label) -> bool {
        if (toks.size() < 2) { feedback = "USAGE: " + cmd + " 0-100"; return false; }
        const float pct = (float) toks[1].getDoubleValue();
        setNormPercent(apvts, id, pct);
        feedback = label + " " + juce::String((int) juce::jlimit(0.0f, 100.0f, pct));
        return true;
    };
    if (cmd == "density") return macro(IDs::technoDensity, "DENSITY");
    if (cmd == "variation") return macro(IDs::technoVariation, "VAR");
    if (cmd == "syncopation") return macro(IDs::technoSyncopation, "SYNC");
    if (cmd == "fill") return macro(IDs::technoFill, "FILL");
    if (cmd == "swing") return macro(IDs::technoSwing, "SWING");

    if (cmd == "randomize" && toks.size() >= 2) {
        float amt = toks.size() >= 3 ? (float) toks[2].getDoubleValue() * 0.01f : 0.4f;
        amt = juce::jlimit(0.0f, 1.0f, amt);
        const auto t = toks[1].toLowerCase();
        if (t == "seq") { randomizeSeq(apvts, amt, feedback); return true; }
        if (t == "timbre" || t == "kit") { randomizeTimbre(apvts, amt, feedback); return true; }
        if (t == "all") {
            juce::String a, b; randomizeSeq(apvts, amt, a); randomizeTimbre(apvts, amt, b);
            feedback = a + " | " + b; return true;
        }
        feedback = "USAGE: randomize seq|timbre|kit|all [0-100]"; return false;
    }
    if ((cmd == "lock" || cmd == "unlock") && toks.size() >= 2)
        return setLockVoice(apvts, toks[1], cmd == "lock", feedback);
    if (cmd == "techno" && toks.size() >= 2) {
        applyTechnoPreset(apvts, toks[1].toLowerCase(), feedback);
        return !feedback.startsWithIgnoreCase("UNKNOWN");
    }
    if (cmd == "dnb" && toks.size() >= 2) {
        applyDnbPreset(apvts, toks[1].toLowerCase(), feedback);
        return !feedback.startsWithIgnoreCase("UNKNOWN");
    }
    if (cmd == "lane" && toks.size() >= 4 && toks[2].equalsIgnoreCase("density")) {
        const auto lane = toks[1].toLowerCase();
        const char* id = nullptr;
        if (lane == "kick" || lane == "bd") id = IDs::bdDensity;
        else if (lane == "snare" || lane == "sd") id = IDs::sdDensity;
        else if (lane == "ch" || lane == "hat" || lane == "hh") id = IDs::hhDensity;
        else { feedback = "LANE DENSITY: kick|snare|ch"; return false; }
        setNormPercent(apvts, id, (float) toks[3].getDoubleValue());
        feedback = "LANE " + toks[1].toUpperCase() + " DENSITY " + toks[3];
        return true;
    }
    feedback = "UNKNOWN: " + cmd + " — try help";
    return false;
}

}  // namespace lxx::ui
