# AGENTS.md

Conventions and hard rules for anyone (human or agent) working in this
repository.

---

## What LXX is

**LXX is a VST3 drum and percussion instrument plugin by RIG-AUDIO.**

It is a MIDI-triggered instrument for hosts. It is *not* a standalone
application, *not* an audio effect, and *not* a melodic synthesiser.

`IS_SYNTH TRUE` is correct and intentional: it means "MIDI-triggered
instrument", which is what a drum machine is. Do not "fix" it.

---

## Hard rules

### 1. VST3 only

* `FORMATS` in `src/plugin/CMakeLists.txt` must contain exactly `VST3`.
* Never add `Standalone`, `AU`, `AAX`, `VST`, or any other format without an
  explicit, written human decision.
* Never introduce an application-style entry point, splash window, or
  "plugin-free" mode.
* `juce_add_console_app(LXXHostRuntimeTest)` is a **diagnostic harness**, not
  a product. It must never be packaged or published as a deliverable.
* Verify with `scripts/verify-plugin-formats.sh`.

### 2. Builds are CI-only

* `LXX_BUILD_MODE` defaults to `ci`; configuring outside GitHub Actions fails
  unless someone runs with `-DLXX_BUILD_MODE=local`.
* Do not build, run `ctest`, `pluginval`, or sanitizers locally unless a human
  has explicitly authorised a local build in the current conversation.
* When authorised, build out of the repository: `-B build`.
* Never publish a locally built binary. Only CI artifacts are real.

### 3. Remote and identity discipline

* Public writes go to exactly one remote:
  `https://github.com/RIG-AUDIO/LXX.git`.
* Before **any** GitHub operation, verify the active account:

  ```sh
  gh auth status --hostname github.com
  gh api user --jq .login      # must print RIG-AUDIO
  scripts/verify-github-identity.sh
  ```

* Never force-push. Never rewrite published history.
* Never write to any other repository as a side effect.
* Never print, log, echo, or commit a token, key, or password. Never
  `cat` credential files.
* Use the repo-local git identity already configured for this repository
  (`user.name` / `user.email` are set with `--local`). Do not change global
  git configuration.

### 4. Licensing and attribution

* The project licence is **GPL-3.0-or-later**. Do not relicense, do not add
  per-file exceptions, do not remove `LICENSE`.
* Preserve the upstream copyright notices for derived Mutable Instruments
  work (`Emilie Gillet / Mutable Instruments`) in the files that carry them.
* Keep `THIRD_PARTY_NOTICES.md` accurate when any dependency changes.
* Keep `src/plugin/resources/Fonts/OFL-1.1.txt` next to the font it covers.
* Do not vendor third-party source without adding it to
  `THIRD_PARTY_NOTICES.md` with its licence.

### 5. Persisted data is frozen

* `src/plugin/parameters/ParameterIDs.h` — automation ids are part of the
  saved state. Never rename, never delete, never repurpose.
* Obsolete parameter ids stay in place so old state keeps loading.
* The value-tree type name and the state blob layout are versioned. Changing
  either requires a migration path, not a cleanup.

### 6. Publishing hygiene

Nothing private or foreign ships in this tree:

* no private machine paths, usernames, or workspace names
* no other company's product names or vendor marks
* no comparisons to unrelated commercial products
* no secrets, key material, credential files, or CI tokens
* no build outputs, IDE state, or editor backups
* no production binaries (`.vst3`, `.so`, `.dll`, `.app`)

Only committed, source-level content may ever be committed. Run
`scripts/verify-publication.sh` before pushing; it is the gate.

### 7. Line endings and encoding

* UTF-8, LF only. `.gitattributes` enforces it.
* No BOM, no `CRLF`, no trailing whitespace churn in a diff.

---

## Code conventions

* C++20, `CMAKE_CXX_EXTENSIONS OFF`.
* Match the surrounding file's style; do not reformat files you are not
  changing.
* `src/dsp` must not include JUCE. Keep the boundary clean — that is what
  makes the test matrix fast.
* Warnings: `-Wall -Wextra -Wpedantic` on GCC/Clang, `/W4 /permissive-` on
  MSVC. Do not silence warnings with blanket pragmas.
* No allocation, locking, or I/O on the audio thread.
* Prefer a small, testable unit in `src/dsp` over logic in the plugin layer.

---

## Testing

```sh
ctest --test-dir build --output-on-failure
```

Every `tests/*.cpp` whose stem does not start with `_` is registered
automatically. A test must not need a host, a device, audio hardware, or
network access. Reference output goes to files the test itself writes, not to
checked-in fixtures.

---

## Before you push

```sh
scripts/verify-github-identity.sh
scripts/verify-plugin-formats.sh
scripts/verify-publication.sh
```

All three must exit 0. If any fails, fix the tree — do not bypass the check,
do not delete the script, do not widen the denylist in a way that hides a real
finding.

---

## Commands

| Task | Command |
| --- | --- |
| Configure (CI) | `cmake -S . -B build` |
| Configure (authorised local) | `cmake -S . -B build -DLXX_BUILD_MODE=local` |
| Build | `cmake --build build --config Release` |
| Test | `ctest --test-dir build --output-on-failure` |
| All gates | `scripts/verify-publication.sh` |
| Which GitHub account | `scripts/verify-github-identity.sh` |
