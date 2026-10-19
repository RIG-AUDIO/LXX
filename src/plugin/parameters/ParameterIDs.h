#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace lxx::params {

struct IDs {
    static constexpr const char* volume = "volume";

    static constexpr const char* tempo = "tempo";
    static constexpr const char* hostSync = "host_sync";
    static constexpr const char* clockDivision = "clock_division";
    static constexpr const char* swing = "swing";
    static constexpr const char* seqRun = "seq_run";

    static constexpr const char* drumsX = "drums_x";
    static constexpr const char* drumsY = "drums_y";
    static constexpr const char* drumsBalance = "drums_balance";
    static constexpr const char* drumsBandwidth = "drums_bandwidth";
    static constexpr const char* bdDensity = "bd_density";
    static constexpr const char* sdDensity = "sd_density";
    static constexpr const char* hhDensity = "hh_density";
    static constexpr const char* bdTone = "bd_tone";
    static constexpr const char* sdTone = "sd_tone";
    static constexpr const char* hhTone = "hh_tone";

    static constexpr const char* drumPage = "drum_page";

    static constexpr const char* bdPitch = "bd_pitch";
    static constexpr const char* bdPitchDecay = "bd_pitch_decay";
    static constexpr const char* bdPitchMod = "bd_pitch_mod";
    static constexpr const char* bdAmpDecay = "bd_amp_decay";
    static constexpr const char* bdCrunch = "bd_crunch";

    static constexpr const char* sdPitch = "sd_pitch";
    static constexpr const char* sdPitchDecay = "sd_pitch_decay";
    static constexpr const char* sdPitchMod = "sd_pitch_mod";
    static constexpr const char* sdAmpDecay = "sd_amp_decay";
    static constexpr const char* sdSnappy = "sd_snappy";

    static constexpr const char* hhPitch = "hh_pitch";
    static constexpr const char* hhAmpDecay = "hh_amp_decay";

    // Kept for old-session migration; runtime always uses native engine.
    static constexpr const char* drumEngine = "drum_engine";
    static constexpr const char* sequencerMode = "sequencer_mode";
    static constexpr const char* midiOut = "midi_out";
    static constexpr const char* seqOnly = "seq_only";

    static constexpr const char* gridsChaos = "grids_chaos";

    static constexpr const char* technoDensity = "techno_density";
    static constexpr const char* technoVariation = "techno_variation";
    static constexpr const char* technoSyncopation = "techno_syncopation";
    static constexpr const char* technoFill = "techno_fill";
    static constexpr const char* technoSwing = "techno_swing";
    static constexpr const char* technoSeed = "techno_seed";

    // Extended native voices — APVTS string IDs preserved for state compatibility
    // (historical "analog_*" names; DSP is native LXX, not Daisy).
    static constexpr const char* ohDecay = "analog_oh_decay";
    static constexpr const char* ohTone = "analog_oh_tone";
    static constexpr const char* lowPitch = "analog_low_pitch";
    static constexpr const char* lowDecay = "analog_low_decay";
    static constexpr const char* lowTone = "analog_low_tone";
    static constexpr const char* midPitch = "analog_mid_pitch";
    static constexpr const char* midDecay = "analog_mid_decay";
    static constexpr const char* midTone = "analog_mid_tone";
    static constexpr const char* lowCharacter = "low_character"; // 0 Auto, 1 Boom, 2 Tom

    // Obsolete Daisy-only IDs retained so old states load without crash (ignored by DSP).
    static constexpr const char* obsoleteKickAttackFm = "analog_kick_attack_fm";
    static constexpr const char* obsoleteKickSelfFm = "analog_kick_self_fm";
    static constexpr const char* obsoleteHatNoisiness = "analog_hat_noisiness";

    // 0 GUI, 1 GUI+CLI, 2 CLI
    static constexpr const char* uiPage = "ui_page";
    static constexpr const char* guiSubPage = "gui_sub_page"; // 0 Main, 1 Advanced

    // Per-voice mixer (dB, default 0) — missing from old sessions → 0 dB
    static constexpr const char* volKick = "vol_kick";
    static constexpr const char* volSnare = "vol_snare";
    static constexpr const char* volCh = "vol_ch";
    static constexpr const char* volLow = "vol_low";
    static constexpr const char* volMid = "vol_mid";
    static constexpr const char* volOh = "vol_oh";

    static constexpr const char* muteKick = "mute_kick";
    static constexpr const char* muteSnare = "mute_snare";
    static constexpr const char* muteCh = "mute_ch";
    static constexpr const char* muteLow = "mute_low";
    static constexpr const char* muteMid = "mute_mid";
    static constexpr const char* muteOh = "mute_oh";

    // Shared GUI/CLI randomization locks
    static constexpr const char* lockKick = "lock_kick";
    static constexpr const char* lockSnare = "lock_snare";
    static constexpr const char* lockCh = "lock_ch";
    static constexpr const char* lockLow = "lock_low";
    static constexpr const char* lockMid = "lock_mid";
    static constexpr const char* lockOh = "lock_oh";
    static constexpr const char* lockTimbre = "lock_timbre";
    static constexpr const char* lockSeq = "lock_seq";
};

}  // namespace lxx::params
