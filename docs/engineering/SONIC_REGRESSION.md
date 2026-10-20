# Sonic regression

How LXX proves that a code change did not change the sound it is supposed to
make, and where the limits of that proof are.

**Rule of thumb used throughout:** numerical claims are only made where a
test measures them. Nothing in this document claims a listening-test
result — no listening study has been run.

---

## What "the sound" is, precisely

The intended sound is fixed by four kinds of invariant:

1. **Hardware tables** — the Anushri firmware lookup tables (glide, drum
   envelope, envelope, LFO increments). These are *the* sound: generated
   offline from the upstream formulas at the hardware control rates and
   compared entry-by-entry in `test_firmware_tables` (256/256 exact per
   table, four tables). A single changed entry fails CI.
2. **Time laws** — glide duration `65536 / (increment × control_rate)`,
   envelope time constants, filter time constants. Asserted as laws, not
   snapshots: `test_glide` measures the sweep at three sample rates;
   `test_envelope`, `test_filter`, `test_vca` assert response shapes with
   tolerances.
3. **Response character** — cutoff monotonicity, HF energy behaviour while
   the filter opens, resonance region, VCA gain curve, VCO pitch/drift,
   LFO shape, drum voice spectral centroid/RMS/attack. Asserted by
   `test_filter`, `test_filter_forensics`, `test_vca`, `test_vco`,
   `test_lfo`, `test_drums`, `test_voice_path`.
4. **Structural sanity** — exact silence at idle, no self-oscillation from
   zero input by default, no NaN/Inf at any block size, sample-rate
   invariance of drum duration and pitch, bounded amplitude and DC under
   long runs. Asserted by `test_idle_silence`, `test_plugin_safety`,
   `test_drums_samplerate`, `test_runtime_stress`, `test_voice_path`.

Precision class and tolerance per block are documented in
[`HARDWARE_FIDELITY.md`](HARDWARE_FIDELITY.md) (classes A/B/C/D) and mapped
to upstream in [`ANUSHRI_REFERENCE_MATRIX.md`](ANUSHRI_REFERENCE_MATRIX.md).

## What actually runs on every change

| Stage | Runs | What it would catch |
| --- | --- | --- |
| `linux.yml` | 26 tests × 4 configurations | any table/law/response assertion breaking, on both compilers, both build types |
| `sanitizers.yml` | same suite under ASan+UBSan ×2 | UB or memory errors that silently change arithmetic |
| `nightly-qualification.yml` | full matrix again + pluginval + sanitizer pass | drift that only shows up over days |
| `pluginval` (strictness 5 and 10) | the built VST3 | host-facing parameter/state/rendering faults |
| `fuzz.yml` | libFuzzer over `lxx_dsp`, 60 s per PR + 600 s nightly | non-finite or crashing inputs in the DSP core |
| `test_cpu_bench` | per run | realtime-factor regressions in the core units |

Reference dumps (`*.csv`, `*.wav`) are produced *by the tests themselves*
into the build tree on demand; they are diagnostic artefacts, not checked-in
golden files (rule 6 in `AGENTS.md`). The assertions compare against values
computed inside the test from the same physical laws, so a "regolden" that
would mask a change is structurally impossible.

## Known limits (honest)

* **No A/B listening evidence.** Class B/C blocks are asserted to their
  law within tolerance; nobody has yet listened to LXX against the
  hardware. Any statement about audibility is currently an engineering
  judgement, not a measurement.
* **No cross-version golden spectra.** There is no stored spectrum from a
  previous release to diff against; detection relies on the invariants
  above.
* **`dsp::Voice` melodic path** is asserted by tests but is not rendered
  by the VST3 chain today (`DSP_ARCHITECTURE.md`), so no host-visible sound
  depends on it yet.
* **CPU budget** is asserted as a realtime factor, not a hard latency
  number; absolute worst-case latency depends on the host's block size.
