# Third-party notices

LXX is distributed under the GNU General Public License, version 3 or later
(see [`LICENSE`](LICENSE)).

This file lists every third-party work that is present in, or is pulled in by,
this repository.

---

## 1. Mutable Instruments — Anushri firmware (derived code)

* **Work:** portions of the LXX DSP core are modelled on, and derived from, the
  Mutable Instruments *Anushri* firmware (voice path, VCO/VCF/VCA behaviour,
  envelope curves, sub-oscillator, panel parameter mappings).
* **Author / copyright:** Copyright (C) 2011-2013 Emilie Gillet / Mutable
  Instruments.
* **Upstream:** <https://github.com/pichenettes/eurorack>
* **Upstream licence:** GNU General Public License, version 3 (stated in the
  upstream Anushri README: *"The firmware is released under a GPL3.0 license."*).
* **Reuse terms:** GPLv3 or later, unchanged. This distribution is therefore
  GPLv3-or-later as a whole.
* **Files carrying the upstream copyright notice or an explicit derivation
  notice:**
  * `src/dsp/filters/svf_anushri.h`, `src/dsp/filters/svf_anushri.cpp`
  * `src/dsp/filters/svf_anushri_nonlinear.h`, `src/dsp/filters/svf_anushri_nonlinear.cpp`
  * `src/dsp/filters/ssm2164_model.h`
  * `src/dsp/amplifiers/vca_as2164.h`, `src/dsp/amplifiers/vca_curve.h`
  * `src/dsp/envelopes/envelope_curves.h`
  * `src/dsp/oscillators/blep_oscillator.h`, `src/dsp/oscillators/sub_oscillator.h`
  * `src/dsp/voice/voice.h`, `src/dsp/voice/voice.cpp`
  * `src/dsp/drums/drum_reconstruction_filter.h`

  The file names `svf_anushri*` are deliberate: they identify the circuit
  topology that the module models. They are provenance, not product branding.

---

## 2. Mutable Instruments — Grids (ported code)

* **Work:** the `grids` drum-pattern generator ported to a host-portable,
  instance-based C++ implementation, together with the `node_0` … `node_24`
  pattern resource tables.
* **Author / copyright:** Copyright (C) 2011-2012 Emilie Gillet / Mutable
  Instruments.
* **Upstream:** <https://github.com/pichenettes/eurorack> — `grids/`
* **Upstream licence:** GNU General Public License, version 3 (the AVR firmware
  in `pichenettes/eurorack` is GPLv3).
* **Reuse terms:** GPLv3 or later, unchanged.
* **Files:**
  * `src/dsp/sequencer/grids/grids_pattern_generator.h`
  * `src/dsp/sequencer/grids/grids_resources.cpp`
  * `src/dsp/sequencer/grids/grids_resources.h`
  * `src/dsp/sequencer/grids/PROVENANCE.txt` — porting record

  AVR / hardware / EEPROM dependencies were removed; the ported code is
  instance-based and free of platform assumptions.

---

## 3. JUCE framework (build-time dependency, not vendored)

* **Work:** the plugin wrapper layer (VST3 entry points, editor plumbing,
  parameter value-tree helpers, message thread dispatch).
* **Copyright:** The JUCE contributors / Raw Material Software.
* **Licence:** GPL-compatible GPL-3.0 (JUCE is dual-licensed; the GPL option is
  what applies to this GPLv3 project).
* **How it is obtained:** `FetchContent` at configure time from
  <https://github.com/juce-framework/JUCE.git>, pinned to tag `7.0.10`
  (commit `a8ae6edda6d3be78a139ec5e429dc57ef047e82a`).
* **Not present in this repository.** No JUCE source is copied into the tree.

---

## 4. Alte DIN 1451 Mittelschrift font (bundled binary asset)

* **File:** `src/plugin/resources/Fonts/din1451alt.ttf`
* **SHA-256:**
  `3cdbdd35c7637ff4a15d22bdcb8b952f48afadd2e41edb5ab29d80eaad8d48fd`
* **Author:** Peter Wiegel — <www.peter-wiegel.de>
* **Licence:** SIL Open Font License, Version 1.1 — full text bundled at
  `src/plugin/resources/Fonts/OFL-1.1.txt`.
* **Reserved Font Name:** "Alte DIN 1451"
* **Use:** linked into the plugin as JUCE binary data for the terminal-style
  editor typeface. The OFL permits bundling; the font file is redistributed
  unmodified.

---

## 5. TPT / state-variable filter topology (published algorithm)

* **Reference:** Andrew Simper (Cytomic), *"Topology-preserving transform
  structures for linear and non-linear one-pole, two-pole and N-pole filters"*.
* **Use:** the TPT recursion written down in `src/dsp/filters/svf_anushri.h`
  is a citation of a published topology. No Cytomic source code is copied.
* **Licence implication:** none beyond the citation recorded in the file
  header.

---

## 6. Upstream Anushri hardware simulations (consulted, not redistributed)

* **Work:** `anu/hardware_design/simulations/` in the upstream Anushri
  repository — the analogue VCO/VCF/pot-taper simulations whose closed-form
  results the DSP control laws were cross-checked against.
* **Upstream licence:** Creative Commons Attribution-ShareAlike 3.0 (the
  upstream Anushri README states that PCB layouts, schematics, documentation,
  analyses, simulations and 3D models are released under CC-BY-SA 3.0, while
  the firmware is GPLv3).
* **Present in this repository?** **No.** Those simulation sources are not
  copied, ported, or redistributed here.
* **What is in this repository instead:** the control laws are written from
  the AS2164 / SSM2164 datasheet equations (60 mV/decade exponential law,
  Boltzmann constant, temperature terms), which are facts and published
  device equations. Code comments that historically named individual
  simulation scripts were rewritten so that no reference points at a file
  that this repository does not ship.
* **Action:** if any future change copies text or structure from those
  simulations, add them to this file with the CC-BY-SA 3.0 terms and satisfy
  its attribution and share-alike requirements first.

---

## 7. dr_wav (optional test-time dependency, not vendored)

* **Work:** WAV output helper, used only if CMake resolves `find_package(drwav)`.
* **Author:** David Reid (mackron/dr_libs).
* **Licence:** MIT / public-domain dual licence.
* **Not present in this repository.** The test suite degrades to CSV-only
  output when it is absent, and no test result depends on it.

---

## Summary

| Component | Present in tree | Licence | Effect on LXX |
| --- | --- | --- | --- |
| LXX first-party source | yes | GPLv3+ (own grant) | — |
| Mutable Instruments Anushri firmware (derived) | yes | GPLv3 | keeps whole work GPLv3+ |
| Mutable Instruments Grids (ported) | yes | GPLv3 | keeps whole work GPLv3+ |
| Alte DIN 1451 font | yes (binary) | SIL OFL 1.1 | compatible; redistributed unmodified |
| Anushri hardware simulations | **no** (consulted only) | CC-BY-SA 3.0 upstream | not redistributed |
| JUCE | no (fetched at configure) | GPLv3-compatible | compatible |
| dr_wav | no (optional) | MIT/public domain | compatible |
