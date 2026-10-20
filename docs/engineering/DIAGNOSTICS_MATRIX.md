# Diagnostics matrix

What each analysis runs in CI, what it gates, what is deliberately not
run, and the status of the two diagnostics findings from the engineering
baseline (P5 positive control, P6 diagnostics-package adoption).

## Stages that run today

| Stage | Where | Scope | Gate semantics |
| --- | --- | --- | --- |
| Compiler warnings, GCC | `linux.yml` (gcc × Debug/Release) | `-Wall -Wextra -Wpedantic -Wno-unused-parameter` on `lxx_dsp` and every test; JUCE's recommended warning set on plugin targets | first-party targets are compiled with `-Werror` (`LXX_WARNINGS_AS_ERRORS=ON`) — a new warning fails the job |
| Compiler warnings, Clang | `linux.yml` (clang × Debug/Release) | as above | as above |
| clang-tidy | `static-analysis.yml` | compile database, first-party sources | findings fail the job |
| cppcheck | `static-analysis.yml` | `src/dsp tests`, `--enable=warning,portability,performance`, C++20 | **error-severity** findings fail the job; advisory findings print and are tracked in `OPEN_FINDINGS.md` |
| AddressSanitizer | `sanitizers.yml` (gcc and clang, Debug) | full test suite, `detect_leaks=1:halt_on_error=1` | any finding fails |
| UndefinedBehaviorSanitizer | `sanitizers.yml` (same builds) | `-fsanitize=undefined -fno-sanitize-recover=undefined`, `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1` | any finding fails |
| Suppression-file ban | `sanitizers.yml` | repository scan for `*.supp` | a suppression file anywhere fails the job — findings are fixed, not muted |
| Sanitizer positive control | `sanitizer-positive-control.yml` | deliberately injected UB, workspace-only | **passes only if the sanitizer catches the injected UB** (inverted assertion) |
| HWASan capability probe | `sanitizers.yml` (clang) | compile-and-run of a trivial probe program | informational: prints an explicit `SUPPORTED` / `UNSUPPORTED — ENVIRONMENT REQUIREMENTS NOT MET` verdict |
| libFuzzer | `fuzz.yml` | `LXXFuzzTarget` over parameter space | findings fail |
| pluginval | `plugin-validation.yml`, `nightly-qualification.yml` | the built VST3, strictness 5 and 10 | failures fail |
| Publication gate | `public-audit.yml`, and as step 1 of `nightly-qualification.yml` | every file that would ship, plus identity/format scripts | any private/foreign content finding fails |

## P5 — sanitizer positive control (resolved)

**Baseline finding.** The sanitizer stage ran green, but the original
positive control — a known-bad input proving the stage can actually go
red — was lost in a pre-baseline baseline and was never stood up on the
public repository. A green sanitizer stage with no proof it can fail
proves nothing about its sensitivity.

**Fix.** `.github/workflows/sanitizer-positive-control.yml`:

1. Fails if `tests/test_lxx_positive_control.cpp` is tracked by git — the
   injected UB must never enter the tree, where it would fail every
   normal suite.
2. Writes that file into the runner's workspace at run time: a
   `volatile` signed-integer overflow (UBSan) followed by a heap overrun
   (ASan, for ASan-only configurations).
3. Configures and builds it with the **exact** flags and environment of
   `sanitizers.yml` (clang/gcc, Debug, `LXX_ENABLE_ASAN=ON`,
   `-fsanitize=undefined -fno-sanitize-recover=undefined`, the same
   `ASAN_OPTIONS`/`UBSAN_OPTIONS`).
4. Runs it and asserts, in inverted form:
   * the exit status is **non-zero**, and
   * the output contains `runtime error:`, and
   * the output contains `signed integer overflow`.

If any of the three fails — sanitizer silent, process exits 0, wrong
diagnostic — the job fails with an explicit message. The control therefore
tests the stage, not the product.

**Evidence.** Run `39398435156` (pull request, head `b9d43cb`) — success on
both matrix compilers; repeated green at head `5f5e351`.

**Scope limits.** The control proves UBSan reports and halts a signed
overflow and ASan is linked in the same configuration; it does not probe
every sanitizer class (use-after-free, leaks, thread issues each have
their own detection path — leaks are additionally covered by
`detect_leaks=1` in the main suite).

## P6 — diagnostics package adoption (partial, deliberate)

An internal diagnostics audit package offers a per-target, opt-in CMake
pattern (`DIAG_WARNINGS_AS_ERRORS` per target, applied through a helper
function, no mutation of global `CMAKE_CXX_FLAGS`).

**Adopted.** The same pattern, implemented natively in the public tree:
`LXX_WARNINGS_AS_ERRORS` (default `OFF`) plus the `lxx_werror(<target>)`
function in the root `CMakeLists.txt`, applied to `lxx_dsp`, every test
executable, `LXXPlugin` and `LXXHostRuntimeTest`. Third-party targets are
never passed through it, so JUCE and the VST3 SDK keep their warnings as
warnings.

**Not adopted.**

* The package itself is not vendored (no third-party source without a
  `THIRD_PARTY_NOTICES.md` entry, and the package is internal tooling).
* No package-internal paths or names are published in this repository.
* Coverage instrumentation (`llvm-cov`/`gcov`) and ThreadSanitizer stages
  the package could also wire up are not yet enabled — see
  `OPEN_FINDINGS.md`.

## Deliberately not run

| Stage | Why not (yet) |
| --- | --- |
| ThreadSanitizer | `src/dsp` is single-threaded by design; no stage asserts that today — tracked |
| Line/branch coverage | no `llvm-cov`/`gcov` stage exists yet — tracked |
| HWASan as a build stage | x86_64 HWASan is experimental and hardware-gated (Intel LAM); the probe reports capability instead of guessing — tracked |
| MSVC / Windows / macOS builds | no runners in the matrix; VST3 format contract is Linux-validated |
| UI/editor unit tests | editor coverage is pluginval's editor instantiation only — tracked |

## Rules that keep the matrix honest

* Warnings are never silenced with blanket pragmas (AGENTS.md).
* No `*.supp` suppression file may exist in the tree.
* Passing tests' full output is preserved: `LastTest.log` is uploaded as
  an artefact by `linux.yml`, `sanitizers.yml` and
  `nightly-qualification.yml`, so measured values behind budgeted claims
  are public evidence rather than a `--output-on-failure` casualty.
