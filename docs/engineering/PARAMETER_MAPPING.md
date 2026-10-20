# Parameter mapping

LXX automation IDs (`src/plugin/parameters/ParameterIDs.h`) mapped to their
DSP destination and, where one exists, the Anushri upstream parameter.
IDs are frozen: renaming, deleting or repurposing one breaks saved state
(AGENTS.md §5). Obsolete IDs stay in place and are ignored by the DSP.

## Global / output

| ID | Range | DSP destination | Anushri |
| --- | --- | --- | --- |
| `volume` | 0..1 | master output gain | — (D) |
| `ui_page` | 0..2 | UI routing only (0 GUI, 1 GUI+CLI, 2 CLI) | — |
| `gui_sub_page` | 0..1 | UI routing only (0 Main, 1 Advanced) | — |

## Sequencer / clock

| ID | Range | DSP destination | Anushri |
| --- | --- | --- | --- |
| `tempo` | BPM | `SequencerTransport::setSettings` | `PARAMETER_TEMPO` |
| `host_sync` | 0/1 | external clock vs internal | — (D) |
| `clock_division` | enum | step division | — (D) |
| `swing` | 0..1 | swing amount | `PARAMETER_SWING` |
| `seq_run` | 0/1 | transport run/stop | — (D) |
| `grids_chaos` | 0..1 | Grids pattern chaos | — (D) |

## Drum mix / timbre

| ID | Range | DSP destination | Anushri |
| --- | --- | --- | --- |
| `drums_x` | 0..1 | drum morph X | `PARAMETER_DRUMS_X` |
| `drums_y` | 0..1 | drum morph Y | `PARAMETER_DRUMS_Y` |
| `drums_balance` | 0..1 | `DrumSynth::setBalance` | `PARAMETER_DRUMS_BALANCE` |
| `drums_bandwidth` | 0..1 | `DrumSynth::setBandwidth` | `PARAMETER_DRUMS_BANDWIDTH` |
| `bd_density` | 0..1 | kick density | `PARAMETER_DRUMS_BD_DENSITY` |
| `sd_density` | 0..1 | snare density | `PARAMETER_DRUMS_SD_DENSITY` |
| `hh_density` | 0..1 | hat density | `PARAMETER_DRUMS_HH_DENSITY` |
| `bd_tone` | 0..1 | kick tone | `PARAMETER_DRUMS_BD_TONE` |
| `sd_tone` | 0..1 | snare tone | `PARAMETER_DRUMS_SD_TONE` |
| `hh_tone` | 0..1 | hat tone | `PARAMETER_DRUMS_HH_TONE` |
| `drum_page` | 0/1 | drum parameter page | — (D) |
| `drum_engine` | — | **obsolete** — runtime always uses the native engine | — |

## Bass drum voice

| ID | Range | DSP destination | Anushri |
| --- | --- | --- | --- |
| `bd_pitch` | 0..1 | kick pitch (`lut_res_dco_pitch`) | `DrumPatch::pitch` |
| `bd_pitch_decay` | 0..1 | kick pitch sweep time | `DrumPatch::pitch_decay` |
| `bd_pitch_mod` | 0..1 | kick pitch sweep depth | `DrumPatch::pitch_mod` |
| `bd_amp_decay` | 0..1 | kick amp envelope | `DrumPatch::amp_decay` |
| `bd_crunch` | 0..1 | kick bit crush | `DrumPatch::crunchiness` |

## Snare drum voice

| ID | Range | DSP destination | Anushri |
| --- | --- | --- | --- |
| `sd_pitch` | 0..1 | snare pitch | `DrumPatch::pitch` |
| `sd_pitch_decay` | 0..1 | snare pitch sweep | `DrumPatch::pitch_decay` |
| `sd_pitch_mod` | 0..1 | snare pitch depth | `DrumPatch::pitch_mod` |
| `sd_amp_decay` | 0..1 | snare amp envelope | `DrumPatch::amp_decay` |
| `sd_snappy` | 0..1 | snare noise level | — (D) |

## Hi-hat voice

| ID | Range | DSP destination | Anushri |
| --- | --- | --- | --- |
| `hh_pitch` | 0..1 | hat pitch | `DrumPatch::pitch` |
| `hh_amp_decay` | 0..1 | hat amp envelope | `DrumPatch::amp_decay` |

## Extended native voices (LXX-only)

| ID | Range | DSP destination |
| --- | --- | --- |
| `techno_density` | 0..1 | techno pattern density |
| `techno_variation` | 0..1 | techno pattern variation |
| `techno_syncopation` | 0..1 | techno syncopation |
| `techno_fill` | 0..1 | techno fill amount |
| `techno_swing` | 0..1 | techno swing |
| `techno_seed` | 0..65535 | techno random seed |
| `analog_oh_decay` / `analog_oh_tone` | 0..1 | open-hat voice |
| `analog_low_pitch` / `analog_low_decay` / `analog_low_tone` | 0..1 | low tom voice |
| `analog_mid_pitch` / `analog_mid_decay` / `analog_mid_tone` | 0..1 | mid tom voice |
| `low_character` | 0..2 | 0 Auto, 1 Boom, 2 Tom |

## Retained obsolete IDs (ignored by DSP)

`drum_engine`, `sequencer_mode`, `midi_out`, `seq_only`,
`analog_kick_attack_fm`, `analog_kick_self_fm`, `analog_hat_noisiness`.
These exist so old sessions keep loading; the DSP never reads them.

## Per-voice mixer

`vol_kick`, `vol_snare`, `vol_ch`, `vol_low`, `vol_mid`, `vol_oh` (dB, default
0) and `mute_kick`, `mute_snare`, `mute_ch`, `mute_low`, `mute_mid`,
`mute_oh`. Missing mixer entries default to 0 dB so old sessions sound
identical.

## Randomization locks

`lock_kick`, `lock_snare`, `lock_ch`, `lock_low`, `lock_mid`, `lock_oh`,
`lock_timbre`, `lock_seq` — shared GUI/CLI locks, not audio parameters.
