# LXX

**LXX** is a MIDI-triggered drum and percussion **instrument plugin** for
VST3 hosts, by **RIG-AUDIO**.

It is a full drum machine in a plugin: a modelling DSP core (voices, envelopes,
filters, amplifiers, oscillators, reconstruction filtering), a pattern
sequencer, and a terminal-style editor.

* **Format:** VST3 only
* **Kind:** instrument (`IS_SYNTH`), MIDI input and MIDI output enabled
* **Categories:** `Instrument` / `Drum`
* **Plugin identifier:** manufacturer code `RgAu`, plugin code `Lxx1`,
  bundle id `com.rigaudio.lxx`
* **Licence:** GNU GPL version 3 or later

LXX is not a standalone application. It is a plugin: it has no application
window, no plugin-free mode, and no other delivery form.

---

## Layout

```
CMakeLists.txt          root build (options, JUCE fetch, subdirectories)
src/dsp/                lxx_dsp — the DSP core, a plain C++ static library
src/plugin/             the LXX VST3 target, editor, parameters, UI
tests/                  lxx_dsp test executables + CTest registration
.github/workflows/      CI: build, plugin validation, sanitizers, audits
scripts/                identity, source and format verification helpers
.githooks/              pre-push guards
```

`src/dsp` has no JUCE dependency at all. It compiles with a plain C++20
compiler, which is what makes the test matrix fast.

---

## Building

Builds happen in CI. A plain configure outside CI is refused unless a human
explicitly authorises it:

```sh
cmake -S . -B build                        # FATAL_ERROR: LXX_BUILD_MODE=ci
cmake -S . -B build -DLXX_BUILD_MODE=local # authorised local build
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

Options:

| Option | Default | Meaning |
| --- | --- | --- |
| `LXX_BUILD_MODE` | `ci` | `ci` builds only on GitHub Actions; `local` requires explicit authorisation |
| `LXX_BUILD_TESTS` | `ON` | build the `lxx_dsp` test executables |
| `LXX_BUILD_PLUGIN` | `ON` | build the LXX VST3 (fetches JUCE `7.0.10`) |
| `LXX_ENABLE_ASAN` | `OFF` | AddressSanitizer build, never for shipping |

The VST3 artifact is written to
`build/src/plugin/LXXPlugin_artefacts/Release/VST3/LXX.vst3`.

### Prerequisites

* CMake ≥ 3.22
* A C++20 compiler (GCC or Clang on Linux; MSVC 2022 on Windows)
* `git` (JUCE is fetched at configure time)
* X11/FreeType/GL development packages on Linux for the editor build

---

## Verifying before you publish anything

```sh
scripts/verify-publication.sh
```

runs every gate locally: GitHub identity, VST3-only formats, absence of
private or third-party branding, licence and notice presence, and the
publication denylist.

`scripts/verify-github-identity.sh` proves which GitHub account is active.
Public writes are only permitted when the active account is `RIG-AUDIO` and
the remote URL is exactly `https://github.com/RIG-AUDIO/LXX.git`.

---

## Parameters and host sessions

Parameter identifiers, ranges and the plugin state layout are stable and
versioned; the automation units and their internal names are part of the
persisted format and are not renamed for cosmetic reasons.

LXX assigns its VST3 identifiers (`RgAu` / `Lxx1` / `com.rigaudio.lxx`) at
first public release. Sessions saved by any pre-release build of this code
are **not** guaranteed to be recalled automatically by the host, because a
pre-release build may have carried different identifiers. Recreating such a
session by hand is supported: the parameter values themselves are unchanged.

---

## Documentation

* [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) — every third-party
  component, its licence, and where it lives
* [`LICENSE`](LICENSE) — GPLv3-or-later grant and full licence text
* [`ARCHITECTURE.md`](ARCHITECTURE.md) — module structure and data flow
* [`AGENTS.md`](AGENTS.md) — repository conventions and hard rules

---

## Attribution

Portions of the DSP core are derived from the **Mutable Instruments Anushri**
firmware and the **Mutable Instruments Grids** pattern generator by
Emilie Gillet, both released upstream under the GNU General Public License
version 3, and reused here under those terms. The bundled UI font
*Alte DIN 1451* by Peter Wiegel is licensed under the SIL Open Font License
1.1. See [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

```
LXX — Copyright (C) 2026 RIG-AUDIO
SPDX-License-Identifier: GPL-3.0-or-later
```
