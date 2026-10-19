# Architecture

LXX is two layers with a hard boundary between them.

```
┌──────────────────────────────────────────────────────────────┐
│ src/plugin  — JUCE VST3 target "LXX"                         │
│   PluginProcessor / PluginEditor / parameters / ui           │
│   owns: AudioProcessorValueTreeState, MIDI in/out, editor    │
└──────────────────────────┬───────────────────────────────────┘
                           │  plain C++20, no JUCE types cross this line
┌──────────────────────────▼───────────────────────────────────┐
│ src/dsp  — static library "lxx_dsp"                          │
│   no JUCE, no I/O, no allocation on the audio thread         │
└──────────────────────────────────────────────────────────────┘
```

`lxx_dsp` deliberately has zero dependency on JUCE. Every unit test in
`tests/` links only `lxx_dsp`, which is what makes the CI test matrix fast and
makes the DSP testable without a host.

---

## `src/dsp`

| Directory | Responsibility |
| --- | --- |
| `core/` | scalar types, DC blocker, oversampler, parameter smoother, MIDI chunk helpers |
| `oscillators/` | band-limited (BLEP) main oscillator, sub-oscillator, VCO model |
| `filters/` | TPT state-variable filter core, nonlinear variant, SSM2164 gain model, oversampled wrapper, self-oscillator |
| `amplifiers/` | AS2164 gain-law VCA model and its curve |
| `envelopes/` | ADSR and the alternative decay curves |
| `lfo/` | LFO shapes |
| `modulation/` | parameter-to-CV scaling |
| `voice/` | note stack, glide, patch, patch→voice mapping, monophonic voice path |
| `drums/` | drum synthesis, drum patches, resources, reconstruction filter |
| `engines/` | engine bus, drum engine, events/lanes |
| `sequencer/` | transport, clock, step/techno/drum sequencers, router, arpeggiator, `grids/` port |

### Audio-thread rules

* No heap allocation, no locks, no syscalls inside `process()`.
* Non-finite values are sanitized at the point of use (filter integrator state
  and cutoff are clamped; the DC blocker and smoother are guards).
* Frequency-response fitting is *never* done per-sample or per-cutoff update;
  coefficients are computed analytically or from a precomputed table.

### `sequencer/grids/`

A host-portable port of the Mutable Instruments *Grids* pattern generator.
AVR / `PROGMEM` / EEPROM dependencies are removed and the generator is
instance-based instead of static. See `sequencer/grids/PROVENANCE.txt` and
`THIRD_PARTY_NOTICES.md`.

---

## `src/plugin`

| File | Responsibility |
| --- | --- |
| `PluginProcessor.h/.cpp` | `LXXAudioProcessor` — owns the APVTS, MIDI I/O, transport, DSP block, state serialisation |
| `PluginEditor.h/.cpp` | `LXXAudioProcessorEditor` — terminal-style editor, header, layout |
| `parameters/ParameterIDs.h` | **the persisted parameter identifiers. Stable. Not cosmetic.** |
| `parameters/ParameterLayout.*` | parameter construction and ranges |
| `ui/` | look-and-feel, knob, skin palette, panel layout, sequencer grid, terminal view/router, command console |
| `test_host_runtime.cpp` | hostless diagnostic harness (`LXXHostRuntimeTest`), a console program used to exercise the real processor without a host |

### Parameter stability

`src/plugin/parameters/ParameterIDs.h` defines the automation unit ids. They
are part of the persisted plugin state and of every saved host session.
Renaming one is a data-format change, not a refactor. Obsolete ids are
**kept** so that older state can still be read.

The same rule applies to the value-tree type name and to the state blob
layout.

### Plugin identity

Assigned once, at first public release, in `src/plugin/CMakeLists.txt`:

```
COMPANY_NAME            RIG-AUDIO
BUNDLE_ID               com.rigaudio.lxx
PLUGIN_MANUFACTURER_CODE  RgAu   (0x52674175)
PLUGIN_CODE               Lxx1   (0x4c787831)
PRODUCT_NAME            LXX
FORMATS                 VST3
```

`FORMATS` must never gain `Standalone` or any other target: LXX is a plugin
only. `JucePlugin_ManufacturerCode` / `JucePlugin_PluginCode` in the
`LXXHostRuntimeTest` definitions must stay in sync with these values.

---

## `tests/`

`tests/CMakeLists.txt` globs `*.cpp` (skipping any file whose stem starts
with `_`) and registers each one with CTest, linked against `lxx_dsp` only.

Tests write CSV/WAV traces for offline analysis; they never depend on a host,
a device, or network access. `analyze_blep.py` reads those traces from a
directory given on the command line or via `LXX_BLEP_CSV_DIR`.

---

## Build graph

```
CMakeLists.txt
├── src/dsp         → lxx_dsp        (always)
├── tests           → test_*         (LXX_BUILD_TESTS)
└── src/plugin      → LXXPlugin      (LXX_BUILD_PLUGIN, fetches JUCE 7.0.10)
                     ├── LXXFonts        (JUCE binary data)
                     └── LXXHostRuntimeTest (console diagnostic harness)
```

`LXX_BUILD_MODE` and the CI-only guard are documented in `README.md`.
