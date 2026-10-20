# DSP architecture

Data flow through the plugin at block granularity. The module/directory map
and the layer boundary live in [`ARCHITECTURE.md`](../../ARCHITECTURE.md);
this document describes what actually runs per host block and how the
sequencer events reach the engines. Line references are to the current tree.

---

## Audio callback

`LXXAudioProcessor::processBlock` (`src/plugin/PluginProcessor.cpp:270`):

```
ScopedNoDenormals
buffer.clear()
incoming MIDI copied aside, output buffer cleared
syncFromParameters / handleTransportParameters / captureHostClock

for each host-sized chunk (nextHostChunkSize, ≤ drumScratch_ capacity):
    dispatchMidiForChunk        host MIDI → note events for this chunk
    sequencer_.process          SequencerRouter (see below)
        → eventScratch_         DrumEventBuffer + arp/sequence note events
    applyDrumEvents             triggers DrumEngineBus voices
    midiOut_.process            arp/sequence notes → outgoing MIDI
    emitMidiOutput              writes to the host's output buffer
    engineBus_.processBlock     renders the 6-part drum bus (or silence)
    drum bus × 0.55 × volume, finiteness guard
    per-channel DcBlocker → destination buffer
```

Two invariants are enforced inline:

* **Chunking.** A host block of any size is split so that no engine scratch
  buffer ever needs to grow (`nextHostChunkSize` + `Voice`-style internal
  chunking in the library paths).
* **Finiteness.** Every drum-bus sample and every output sample is checked
  with `std::isfinite`; a non-finite sample becomes `0.0f`. Denormals are
  flushed by `ScopedNoDenormals`.

## Drum engine bus

`DrumEngineBus` (`src/dsp/engines/engine_bus.h`) owns six parts —
kick, snare, hat, low, mid, open hat — each with level (`volDb`) and mute:

* `core_` — `LegacyDrumEngine` → `drums/drum_synth.h`, the Anushri-derived
  drum synthesis (fidelity classes A/B in `HARDWARE_FIDELITY.md`).
* `extended_` — `drum_engine.h` → `drums/native_extended_voices.h`, the
  native LXX voices (class D).
* `scratch_` — block scratch buffer, reserved in `init()` at
  `max(2 × block, 4096)` samples; the grow-on-demand path is documented as
  a last resort (see `REALTIME_SAFETY.md`).

The block is rendered once into `scratch_`, then mixed, DC-blocked and
finiteness-checked in the plugin layer.

## Sequencer

```
SequencerRouter::process
  └── sequencer mode (legacy / grids / techno / dnb …)
        └── SequencerTransport::processBlock
              ├── clock (internal tempo or host PPQ)
              └── dispatchStep  — ≤ 16 step pulses per block
                    ├── DrumSequencer::clockStep  → FixedEventList<DrumHitEvent, 3>
                    └── TransportEvents           → fixed arp / sequence / drum lists
```

* **Drum events** (`eventScratch_`) trigger `DrumEngineBus` voices in the
  same block (offset-adjusted per chunk).
* **Arpeggiator and step-sequencer note events** do not render audio in the
  plugin; they leave through `midiOut_` as host MIDI output — the
  melodic sequence is for the host, the audio is drums.
* All event lists are `FixedEventList` (fixed capacity, allocation-free);
  capacities are derived from structural bounds — see
  `REALTIME_SAFETY.md`.

## Library-only paths

`src/dsp/voice/` (note stack, glide, patch mapping, monophonic `Voice`
render path) is a fully tested part of the `lxx_dsp` library —
`test_voice`, `test_voice_path`, `test_glide`, `test_pitch`,
`test_note_stack` — but it is **not instantiated by the current VST3
chain**: no file under `src/plugin` includes `voice/voice.h`. Its hardware
fidelity (glide law, envelope law, control rates) is still verified to the
same standard because it is part of the same library and will be reachable
by future engine work.

## Control rates and tables

The Anushri-derived blocks run at the hardware control rates
(voice `2450.9375` Hz, drum `1225.46875` Hz — the Python-2 integer-division
values), with lookup tables generated from the upstream formulas and
asserted bit-exact by `test_firmware_tables`. See
`ANUSHRI_REFERENCE_MATRIX.md` for the per-block mapping.
