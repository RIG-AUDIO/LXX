# Realtime safety

What is allowed to run inside the audio callback, what is proven about it,
and what is not yet proven. The rules themselves are stated in
[`ARCHITECTURE.md`](../../ARCHITECTURE.md) ("Audio-thread rules") and in
`AGENTS.md`; this document records the *evidence*.

---

## The audio-thread chain

Everything below runs inside `LXXAudioProcessor::processBlock`
(`src/plugin/PluginProcessor.cpp`), once per host block, split into
host-sized chunks by `nextHostChunkSize`:

```
LXXAudioProcessor::processBlock
  ├── syncFromParameters / handleTransportParameters / captureHostClock
  ├── dispatchMidiForChunk            (per chunk; fills fixed-capacity scratch)
  ├── sequencer_.process              (SequencerRouter)
  │     └── LegacySequencerMode::process
  │           └── SequencerTransport::processBlock
  │                 └── dispatchStep  (≤ 16 step pulses per block)
  │                       ├── DrumSequencer::clockStep  → FixedEventList (≤ 3)
  │                       └── TransportEvents           → fixed lists
  ├── applyDrumEvents / engineBus_.processBlock
  ├── midiOut_.process / emitMidiOutput
  └── per-channel DcBlocker + finiteness guard → output buffer
```

The neighbouring `Voice` path (`src/dsp/voice/voice.cpp`) renders through
`Voice::processBlock`, which **chunks oversized blocks internally** rather
than resizing any buffer.

---

## Finding B10 — heap allocation on the audio thread (fixed)

**Symptom.** The sequencer transport built its per-step event lists with
`std::vector`. Every step pulse therefore allocated (and later freed) on the
audio thread, and `TransportEvents` itself carried three `std::vector`
members that were constructed and `insert`ed per pulse.

**Impact.** Up to 16 step pulses per block × allocations = worst-case 16
allocations per block in the clocked path, reachable from the audio callback
for a playing sequencer. On a realtime thread this is a potential priority
inversion with the allocator lock.

**Fix** (commits `406d7b5`, `05a0bff`, `3390527`):

* `FixedEventList<T, Capacity>` in `src/dsp/sequencer/sequencer_types.h` —
  a fixed-capacity, allocation-free event list with `empty / size / capacity /
  operator[] / clear / push / append / begin / end`, backed by `std::array`.
* `DrumSequencer::clockStep`, `Arpeggiator::clockStep` and
  `StepSequencer::clockStep` now return fixed lists; `TransportEvents` holds
  fixed lists; `transport.cpp` uses `append` instead of `insert`.
* Capacities follow *structural* bounds of the transport, not guesses:

  | Constant | Value | Bound it comes from |
  | --- | --- | --- |
  | `kMaxStepPulsesPerBlock` | 16 | `SequencerTransport::processBlock` slice size |
  | `kMaxTransportDrumEvents` | 64 | 16 pulses × `kNumDrumParts` (3) = 48, plus headroom; still below the 96-event engine buffer |
  | `kMaxArpEventsPerBlock` / `kMaxSequenceEventsPerBlock` | 16 | one event per pulse maximum |
  | `kMaxArpEventsPerStep` | 4 | arpeggiator octave span |

* `EngineBus::init` now `reserve`s headroom for its scratch buffer
  (`max(2 × block, 4096)`), so the grow-on-demand path does not reallocate
  when a host block exceeds the reported maximum. The grow path itself is
  documented as a last resort and still allocation-based.

**Evidence.** `tests/test_realtime_allocation.cpp` replaces the global
`operator new` / `operator delete` (all forms: sized, array, nothrow,
aligned) with counting wrappers, warms the path up, then asserts that the
allocation *and* free counters stay at zero while

* `SequencerTransport::processBlock` renders 256 × 512-sample blocks with a
  running transport, and
* `DrumSequencer::clockStep` steps 256 times,

and — so the measurement cannot pass on an empty path — that drum events
were actually produced. The test passed in all four CI configurations of
commit `3390527` (gcc/clang × Debug/Release, `100% tests passed, 0 tests
failed out of 26` in each).

---

## What allocates, and where it is allowed

| Site | When | Verdict |
| --- | --- | --- |
| `Voice` envelope/filter scratch | `prepare()` / first block (init-time `assign`) | allowed: audio-thread-free init |
| `Oversampler`, `OversampledFilter` tables | `prepare()` | allowed: init-time |
| `EngineBus::scratch_` | reserved in `init()`; grow-on-demand path exists | reserved headroom; grow path is a last resort (documented in `engine_bus.h`) |
| `MidiMessageBuffer`, `DrumEventBuffer` | fixed-capacity by design | no allocation |
| `TransportEvents` / all sequencer event lists | fixed-capacity (`FixedEventList`) | **no allocation** (verified) |
| Parameter parsing / state | host message thread | not on the audio thread |

Denormal protection uses `juce::ScopedNoDenormals` at the top of
`processBlock`.

---

## Proven / not proven

**Proven (CI):**

* Zero heap allocations in the transport + drum-clock audio path
  (`test_realtime_allocation`, four configurations).
* No `std::vector` remains in any function reachable from `processBlock`
  except the reserved `EngineBus` scratch described above.
* Output finiteness under stress: `test_runtime_stress` renders long runs
  and asserts `nan == 0`, `inf == 0`, bounded amplitude and DC.
* Realtime-relevant wall-clock bounds (5-second transport catch-up, long-run
  completion) are asserted in every configuration.

**Not yet proven (tracked in `OPEN_FINDINGS.md`):**

* The allocation counter covers the *sequencer + drum* path and the
  `DrumSequencer` step; it does not yet wrap the full
  `LXXAudioProcessor::processBlock` (that would require a JUCE-linked host
  test) nor the `Voice` note path.
* No lock analysis (there are no locks in `src/dsp`, but this has not been
  asserted by a dedicated TSan/lock-order stage).
* JUCE-side realtime behaviour (parameter smoothing is `atomic`-loaded,
  but the host callback itself is outside `lxx_dsp`'s guarantees).
