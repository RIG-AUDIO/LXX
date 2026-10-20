# Anushri reference matrix

Every LXX DSP block traced to its Anushri upstream counterpart, with the
fidelity classification used in `HARDWARE_FIDELITY.md`:

* **A — bit-exact / numerically identical** (same constants, same arithmetic
  order, asserted in CI).
* **B — behaviourally equivalent** (same law, different precision or
  interpolation; audibly indistinguishable, not asserted bit-exactly).
* **C — inspired** (same musical role, different implementation).
* **D — native LXX** (no Anushri counterpart).

Reference: the Anushri firmware upstream at `pichenettes/anushri`, commit
`e030d7b724e143e8689e486c48a453e07f0096ba` (2019-04-02), read-only.
Upstream is GPL-3.0; derived tables keep their attribution.

## Control rates and clocking

| Item | Anushri | LXX | Class |
| --- | --- | --- | --- |
| Voice control rate | 20 MHz / 510 / 8 / 2 = 2450.98039 Hz (hardware); tables generated with the Python 2 integer value 39215/8/2 = 2450.9375 Hz | `kVoiceControlRate = 2450.9375` (table parity), `kFirmwareControlRate` (hardware rate) in `glide.cpp` | A (tables) |
| Drum control rate | 39215 / 32 = 1225.46875 Hz | `kDrumControlRate = 1225.46875` | A |
| Glide counter | `uint16_t pitch_counter_`, wrap = sweep complete (`voice.cc:202-218`) | `uint32_t counter_` in 16.16 form, balance = `counter_ >> 16`, wrap detection identical | A (duration law, `test_glide.cpp`) |
| Envelope counter | `uint16_t` phase, 16-bit value | `uint32_t` phase, 16.16 | B |

## Lookup tables

| Table | Upstream | LXX | Class |
| --- | --- | --- | --- |
| `lut_res_glide_increments` | `resources.cc` | `EnvelopeCurves::generateGlideIncrements` | A — 256/256 exact (`test_firmware_tables.cpp`) |
| `lut_res_drm_env_increments` | `resources.cc` | `generateDrumEnvIncrements` | A — 256/256 exact |
| `lut_res_env_increments` | `resources.cc` | `generateEnvIncrements` | A — 256/256 exact |
| `lut_res_lfo_increments` | `resources.cc` | `generateLfoIncrements` | A — 256/256 exact |
| `lut_res_env_expo` | `resources.cc` | `EnvelopeCurves::expoTable_` (257 points) | B — same curve, different resolution |
| `lut_res_drm_phase_increments` | `resources.cc` | `drum_resources::lut_res_drm_phase_increments` | A — transcribed, not regenerated |
| `lut_res_dco_pitch` | `resources.cc` | `drum_resources` pitch tables | A — transcribed |
| `lut_res_groove_*` | `resources.cc` | `sequencer/grids` | A — transcribed (Grids provenance) |
| `wav_res_lfo_waveforms` | `resources.cc` | `lfo` wavetables | A — transcribed |

## Voice / melodic path

| Block | Anushri | LXX | Class |
| --- | --- | --- | --- |
| Oscillator | `voice.cc` DCO, 16-bit phase, `lut_res_dco_pitch` | `Voice::UnisonOsc`, 32-bit phase | B |
| Pitch envelope | `lut_res_env_increments` driven, 16-bit | `AdsrEnvelope` + `generateEnvIncrements` | A (table), B (precision) |
| Glide / portamento | `lut_res_glide_increments`, `uint16_t` counter | `PitchGlide` | A (duration law) |
| VCF | `svf` TPT, `k = 2*tan(pi*fc/fs)`, `r = 2/k` at self-oscillation | `SVFAnushriLinear` / `SVFAnushriNonlinear` | B — same topology, float32 vs fixed point |
| VCA | AS2164 model | `VcaAs2164` / `VcaCurve` | B |
| Envelope | `envelope.h` ADSR, 16-bit, `lut_res_env_increments` | `AdsrEnvelope` | A (table), B (precision) |
| LFO | `lfo.h`, 8 shapes, `lut_res_lfo_increments` | `Lfo`, 8 shapes | A (table), B (precision) |
| Oversampling | none (AVR) | `Oversampler` (2x/4x) | D |

## Drum path

| Block | Anushri | LXX | Class |
| --- | --- | --- | --- |
| Drum voices | `drum_synth.cc`, 3 instruments, `uint16_t` phase | `DrumSynth`, 3 instruments | B |
| Pitch sweep | `pitch_env_phase` / `pitch_env_increment` | `bdPitchDecay` / `sdPitchDecay` | B |
| Amp envelope | `amp_env_phase` / `amp_env_increment` from `lut_res_drm_env_increments` | `AdsrEnvelope` + `generateDrumEnvIncrements` | A (table), B (precision) |
| Crunch / bit crush | `crunchiness`, sample-rate reduction | `updateBitcrushPeriod` | B |
| Balance / bandwidth | `PARAMETER_DRUMS_BALANCE`, `PARAMETER_DRUMS_BANDWIDTH` | `setBalance`, `setBandwidth` | B |
| Density | `PARAMETER_DRUMS_*_DENSITY` | `bdDensity` / `sdDensity` / `hhDensity` | B |

## Known divergences (documented, not defects)

1. **Glide seconds label is non-linear.** The firmware index→time law is a
   gamma law over 0.001224 s … 6.6847 s. LXX maps `glideTimeSec` linearly
   onto index 0..255, which reproduces the *panel* domain exactly
   (50 % → index 128 → 0.2105 s). Changing the range would disturb saved
   automation, so the mapping stays and the label non-linearity is documented.
2. **Python 2 integer division.** The published tables were generated with
   `20000000 / 510 == 39215` before the float divisions. LXX reproduces the
   published tables exactly (2450.9375 Hz / 1225.46875 Hz) rather than the
   true hardware rate (2450.98039 Hz); the 1.7e-5 relative difference is part
   of the firmware sound.
3. **16.16 vs 16-bit counters.** LXX holds envelope and glide phases in
   32-bit 16.16 form so the sweep can advance per audio sample instead of per
   control period. Duration laws are identical (`test_glide.cpp`).
4. **Dead generators.** `generateDrumEnvIncrements` and
   `generateLfoIncrements` have no runtime caller today (the drum engine uses
   the committed `drum_resources` tables). They are fixed and pinned by the
   parity test so they stay correct when a future path activates them.
