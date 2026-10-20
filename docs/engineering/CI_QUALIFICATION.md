# CI qualification

What each workflow proves, how the gates relate, and the repair history
of this branch. Run IDs referenced here are runs of this public
repository and can be re-fetched by anyone.

## Workflow inventory

| Workflow | Triggers | Proves | Fails when |
| --- | --- | --- | --- |
| `linux.yml` | push (all branches), PR, dispatch | the full 26-test suite under gcc/clang × Debug/Release with `-Werror` on first-party targets; no Standalone in the build graph; VST3 artefact produced and hashed; `LastTest.log` uploaded | test failure, new first-party warning, Standalone presence, missing artefact |
| `sanitizers.yml` | push (all branches), PR, dispatch | ASan (+leaks) and UBSan (halt-on-error) across the suite, gcc and clang; no suppression files exist; HWASan capability probe verdict; `LastTest.log` uploaded | any sanitizer finding, any `*.supp` file |
| `static-analysis.yml` | push (all branches), PR, dispatch | clang-tidy on the compile database; cppcheck over `src/dsp tests` | any clang-tidy finding; cppcheck **error-severity** findings (advisory findings are printed and tracked) |
| `fuzz.yml` | PR, dispatch, schedule 03:43 UTC | the libFuzzer harness builds and runs against parameter space | any finding |
| `plugin-validation.yml` | push (all branches), PR, dispatch | pluginval strictness 5 and 10 on the built VST3, including editor instantiation | any pluginval failure |
| `public-audit.yml` | push (all branches), PR, dispatch | identity (`RIG-AUDIO` account, single writable remote), plugin-format contract, publication hygiene | identity/format/gate failure |
| `nightly-qualification.yml` | schedule 02:07 UTC, dispatch | publication gate as step 1 (with an explicit token), the four matrix configs, a sanitizer pass on clang/Debug, pluginval, and a packaged qualification report with VST3 hashes; CTest logs uploaded | any constituent step |
| `sanitizer-positive-control.yml` | PR, dispatch, schedule 04:31 UTC Wednesday | **the sanitizer stage can fail**: an injected UB must be caught, or the job fails | sanitizer does not catch the injected UB |

Every workflow runs with `permissions: contents: read` and, where runs can
overlap, `cancel-in-progress` scoped to the workflow and ref.

## Gates run before every push

```sh
scripts/verify-github-identity.sh   # RIG-AUDIO account, LXX-only origin
scripts/verify-plugin-formats.sh    # VST3-only, JUCE pinned, no binaries
scripts/verify-publication.sh       # full-tree publication gate
```

All three must exit 0; the same publication script is step 1 of the
nightly qualification.

## Repair history on `audit/anushri-fidelity-ci-hardening`

| # | Finding | Fix | Verification |
| --- | --- | --- | --- |
| B1 | identity gate failed inside CI (no local token) | `5304f73` | `public-audit` green |
| B2 | Standalone-prohibition was a naive string match | `5304f73` | configured-build-graph gate |
| B3 | `lxx_dsp` not position independent (plugin-validation link) | `5304f73` | `plugin-validation` green |
| B4 | real firmware-table UB caught by UBSan | `649e33f` | sanitizer stage green |
| B5 | wall-clock budgets asserted instead of correctness under gcc | `649e33f` | suite green |
| B6 | wrong VST3 artefact path in `plugin-validation.yml`, `nightly-qualification.yml`, `README.md` | `649e33f` | artefact checks green |
| B7 | clang/Release link failure via `juce_recommended_lto_flags` | `57d702f` | clang/Release green |
| B8 | wall-clock budgets failed in unoptimised builds (scaled budgets) | `57d702f` | Debug configs green |
| B9 | `test_glide` fastest-glide boundary off by one sample of quantisation | `57d702f` | `test_glide` green |
| B10 | heap allocation inside the sequencer audio path | `406d7b5`, `05a0bff`, `3390527` (`FixedEventList` + `test_realtime_allocation`) | zero-allocation assertion green in 4 configs |
| B11a | nightly publication gate lacked a token in CI | `c6bda13` | gate prints `PUBLICATION GATE PASSED` |
| B11b | nightly report step ran `sha256sum` on the `.vst3` **directory** | `5b77481` (`find -type f`) | nightly report packaged |
| W1 | 16 first-party warning sites across 4 configs | `47a2bd3` (behaviour-preserving) | zero first-party warnings |
| W2 | warnings could return silently | `74a3ca9` (`LXX_WARNINGS_AS_ERRORS`, `lxx_werror()`, `-Werror` in `linux.yml`) | linux green with enforcement on |
| C1 | the W1 switch cleanup itself created a missing-return path (gcc `-Wreturn-type`, cppcheck error) | `5f5e351` (trailing return, matching `midiNoteForVoice`) | linux + static-analysis green |
| D | diagnostics: positive control, HWASan probe, CTest-log artefacts | `5b77481` | positive control green (both compilers) |

Each behavioural fix carries a test or a gate; each cleanup carries the
equivalence argument in its commit message. No test was weakened, no
warning was suppressed, and no intended sound change is part of this
history.

## Evidence

Run IDs are listed in `OPEN_FINDINGS.md` alongside the claims they
support (positive-control runs, the `-Werror` matrix, the nightly
qualification, sanitizer passes, and the measured-value artefacts).
