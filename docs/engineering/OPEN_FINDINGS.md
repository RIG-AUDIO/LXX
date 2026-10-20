# Open findings

Everything not yet closed, with the evidence each item is waiting on and
what would close it. Resolved diagnostics findings (P5 positive control,
P6 package adoption) live in `DIAGNOSTICS_MATRIX.md`; the repair history
lives in `CI_QUALIFICATION.md`.

## P7-1 — HWASan: capability probe, not a build stage

**State.** `sanitizers.yml` (clang) now runs a machine-generated
capability probe — `uname -m`, CPU model, the `lam` flag, and a trial
`clang++ -fsanitize=hwaddress` compile/run — and prints an explicit
`SUPPORTED` or `UNSUPPORTED — ENVIRONMENT REQUIREMENTS NOT MET` verdict.
Nothing is skipped silently and nothing fails the build on the answer.

**Background.** Hardware-assisted ASan on x86_64 is an experimental LLVM
path that requires Intel LAM (Linear Address Masking) with kernel
support; without it a HWASan binary cannot run correctly. A previous
qualification report carried the `UNSUPPORTED — ENVIRONMENT REQUIREMENTS
NOT MET` wording as an assertion; the probe turns it into evidence.

**Probe verdict (CI, run `39400667423`, clang job, head `5f5e351`):**

```text
arch: x86_64
model name	: INTEL(R) XEON(R) PLATINUM 8573C
cpu flag lam: absent
HWASan: UNSUPPORTED — ENVIRONMENT REQUIREMENTS NOT MET (builds, does not run)
```

The runner's toolchain accepts the `-fsanitize=hwaddress` flags but the
binary cannot run there: the CPU exposes no `lam` flag, exactly the
hardware dependency LLVM documents for the experimental x86_64 path.

**To close.** If the probe reports `SUPPORTED`, promote HWASan to a real
stage (clang-only job, same suite, no suppressions). If it reports
`UNSUPPORTED`, keep the probe as the standing evidence and re-test only
on a runner image change.

## P7-2 — 5-second transport catch-up: budget enforced, measurement to publish

**State.** `tests/test_runtime_stress.cpp` drives a 5-second catch-up
(`t.processBlock(48000 * 5)`) and asserts the wall clock stays under
`lxx::test::timeBudget(2.0)` in every configuration — a real, enforced
bound.

**Why this is open.** CTest only prints output of *failing* tests
(`--output-on-failure`), so the measured wall time of the passing run
never appeared in any log; the claim was only ever "budget met", never a
published number.

**Change made.** `linux.yml`, `sanitizers.yml` and
`nightly-qualification.yml` now upload `build/Testing/Temporary/LastTest.log`
as artefacts (`if: always()`), which contains full stdout of *passing*
tests too.

**Measured value (first artefact set, run `39400660767`, Release):**

```text
  transport catch-up wall=0.0000s budget=2.00s
```

Identical on gcc and clang. The value prints below the fourth decimal,
i.e. **under 0.00005 s** for a single 5-second (240 000-sample)
`SequencerTransport::processBlock` call — small because per-call work is
bounded by design (clock arithmetic plus a 16-step-pulse dispatch cap per
call), which is exactly what the bound is there to keep bounded. The same
artefact also publishes previously hidden measurements, e.g. the 30-minute
equivalent long run: `blocks=1350000 notes=42188 wall=35.39s` (gcc) /
`wall=63.39s` (clang), `nan=0 inf=0 maxAbs=0.1045 mean=0.000542`.

**To close.** Raise the printed precision of that line (four decimals
cannot distinguish tens of microseconds), publish the resulting value
per configuration, and decide whether a tighter budget is warranted.

## P7-3 — `test_realtime_allocation` scope

**State.** The counting `operator new`/`delete` wrappers assert zero
allocations with a non-empty event stream over the transport and drum
clock path (`TransportEvents`, `clockStep`, `DrumSequencer::step`) in all
four configurations.

**Not covered.**

* the full `LXXAudioProcessor::processBlock` (would need a JUCE-linked
  host harness — `LXXHostRuntimeTest` is the natural place),
* the `Voice` note-on path,
* re-allocation inside `EngineBus::scratch_` growth (documented as a
  last-resort path with reserved headroom, not asserted).

**To close.** Extend the allocation harness to the JUCE-linked
`processBlock`, keeping the test hostless (no device, no audio).

## P7-4 — no coverage stage, no ThreadSanitizer stage, no UI unit tests

| Gap | Status | To close |
| --- | --- | --- |
| Line/branch coverage | not measured anywhere | add `llvm-cov`/`gcov` stage (the internal diagnostics package supports it); decide a floor only after a first honest baseline |
| ThreadSanitizer | not run; `src/dsp` is single-threaded by design but nothing asserts it | add a TSan job over the suite if a second thread ever touches DSP; otherwise record as accepted-by-design with an explicit test later |
| UI/editor unit tests | editor covered only by pluginval's editor instantiation | a JUCE-linked editor test would break the hostless test rule — keep pluginval as the UI gate for now |

## P7-5 — pre-existing cppcheck advisories (4, non-fatal)

Identical on every head since the earliest qualifying runs; the gate is
error-severity, which is clean:

* `VcoCvScaler::invScaleLow_` not initialized in the constructor
  (`warning`)
* `VcoCvScaler::invScaleHigh_` not initialized in the constructor
  (`warning`)
* parameter `p` should be passed by const reference (`performance`, ×2)

**To close.** A small behaviour-preserving cleanup (initialize the two
members, take the two parameters by const reference) with the same
test-backed equivalence argument as the warning cleanup commits.

## P7-6 — one third-party warning, left in place by design

The VST3 SDK's `funknown.cpp` emits a format-truncation warning under
GCC. It compiles inside a third-party target that is deliberately never
passed to `lxx_werror()`, so it cannot fail the build.

**To close.** Only via a JUCE/VST3 SDK version update (a pinned-dependency
decision), never by patching vendored sources in this tree.

## Evidence index

Run IDs supporting the claims in this document (all runs of
`RIG-AUDIO/LXX`, reproducible with `gh run view <id> -R RIG-AUDIO/LXX`;
the pull-request set below is from PR #1 at head `5f5e351`):

| Claim | Run |
| --- | --- |
| positive control catches injected UB (gcc + clang) | `39400666871` (also green at `39398435156`, head `b9d43cb`) |
| `-Werror` matrix green (4 configs) + CTest-log artefacts | `39400667051` (push twin: `39400660767`, artefact source for P7-2) |
| cppcheck error-severity clean, clang-tidy clean | `39400666909` |
| sanitizer pass (ASan + UBSan, 26/26) and HWASan probe verdict | `39400667423` |
| fuzz harness clean | `39400666855` |
| pluginval strictness 5 + 10 clean | `39400667072` |
| publication/identity/format gates clean | `39400666862` |
| nightly qualification green (gate, 4 configs, sanitizer pass, report) | `39398533914` (head `b9d43cb`; re-dispatched on the final head) |
