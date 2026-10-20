# Test coverage

26 CTest executables, all linking only `lxx_dsp` (no JUCE, no host, no
device, no network). Registration is automatic: every `tests/*.cpp` whose
stem does not start with `_` becomes a test in `tests/CMakeLists.txt`.
Stems starting with `_` are reserved for sources that are deliberately *not*
built (see `DIAGNOSTICS_MATRIX.md`).

---

## The suite

| Test | What it proves |
| --- | --- |
| `test_blep` | Band-limited (BLEP) oscillator output shape at high frequency and short pulses (CSV/WAV dumps) |
| `test_cpu_bench` | Realtime factor and µs/block for the core units — the performance baseline |
| `test_drums` | Per-drum voice peak/RMS/attack/centroid characterisation |
| `test_drums_samplerate` | Drum duration and pitch are sample-rate invariant |
| `test_envelope` | ADSR fast/long curves (CSV/WAV) |
| `test_filter` | SSM2164 gain model + SVF cutoff frequency and resonance response |
| `test_filter_forensics` | Effective filter rate is 2× host rate; coefficient updates are realtime-safe; integrator clamps |
| `test_firmware_tables` | All four firmware lookup tables bit-exact (256 entries each) against the upstream generator formulas |
| `test_glide` | Glide law `65536/(increment · control rate)` measured at 3 sample rates; no zero increments |
| `test_idle_silence` | Default idle is exact silence; default resonance does not self-oscillate on zero input |
| `test_lfo` | LFO shape family (bipolar, sweep) |
| `test_lxx_upgrade` | Patch semantics after upgrade: attenuation, bandwidth, boom/tom structural distinctness |
| `test_midi_chunking` | Host MIDI events dispatched exactly once across chunk boundaries (31/32 pair) |
| `test_note_stack` | Note stack capacity 16, duplicate-note replacement, no underflow |
| `test_oversampled_filter` | Oversampled filter settles and stays finite |
| `test_oversampler` | Oversampler stage correctness |
| `test_pitch` | Pitch storage/tracking, legato overlap |
| `test_plugin_safety` | Host-facing safety: block-size sweeps, DC blocker finiteness, per-oscillator integrator state, NaN/Inf counts |
| `test_realtime_allocation` | **Zero heap allocation and free** while the transport renders and the drum sequencer steps, with events proven non-empty |
| `test_runtime_stress` | Long-run finiteness/amplitude/DC bounds, 5-second transport catch-up wall-clock bound, voice/filter/oversampler stress |
| `test_self_osc` | Self-oscillation impulse and step responses |
| `test_sequencer` | Step-pulse capture, arp gate bitmask, generative map activity |
| `test_vca` | AS2164 VCA curve and CV scaling |
| `test_vco` | VCO CV sweep, drift in cents, MIDI-note mapping |
| `test_voice` | Voice integration: single note, legato, glide, self-oscillation |
| `test_voice_path` | Full voice path: finiteness during cutoff sweeps, attack-amplitude monotonicity, idle silence, fuzzed output finite |

Numeric reference material (tables, curves, spectra) is written by the tests
themselves to `*.csv` / `*.wav` in the build tree — nothing binary is
checked in (rule 6 in `AGENTS.md`).

---

## How guarantees are enforced in CI

* **Registration.** `tests/CMakeLists.txt` globs, builds, registers each test
  and sets a per-test CTest `TIMEOUT`.
* **Wall-clock budgets.** Tests that assert "finishes in time" use
  `lxx::test::timeBudget(seconds)`, scaled by `LXX_TEST_TIME_SCALE`:
  `1.0` for normal builds, `16.0` for sanitizer and `-O0` Debug builds.
  Correctness assertions (finiteness, NaN/Inf, DC, amplitude, counts, timing
  accuracy) are **never** scaled. CTest timeouts (900 s / 5400 s) sit above
  the scaled budgets so a test's own watchdog always reports first.
* **Matrices.** The suite runs in `linux.yml` (gcc/clang × Debug/Release),
  in `sanitizers.yml` (ASan+UBSan, gcc and clang), in
  `nightly-qualification.yml` (the same four configurations plus a
  sanitizer pass on clang/Debug), and in `plugin-validation.yml` indirectly
  through pluginval on the real VST3.
* **Exit discipline.** Tests return non-zero on any failed assertion;
  `--no-tests=error` prevents a silently empty suite.

---

## Known gaps

* **Line/branch coverage is not measured.** No `llvm-cov`/`gcov` stage runs
  yet; the diagnostics package supports it (`DIAGNOSTICS_MATRIX.md`).
* **`test_realtime_allocation` scope.** It covers the transport and drum
  clock path, not the full `processBlock` and not the `Voice` note path.
* **No thread-sanitizer stage.** `src/dsp` is single-threaded by design;
  nothing asserts that in CI.
* **No UI/editor tests.** The editor is validated only indirectly through
  pluginval's editor instantiation.
* **Host interoperability** is pluginval's job (strictness 5 and 10), not
  the unit suite's.
