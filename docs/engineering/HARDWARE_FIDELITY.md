# Hardware fidelity classification

How closely each LXX DSP block reproduces the Anushri hardware it descends
from. Classes are defined once here and used in `ANUSHRI_REFERENCE_MATRIX.md`.

| Class | Meaning | Verification |
| --- | --- | --- |
| **A** | Bit-exact or numerically identical: same constants, same arithmetic order, same truncation | CI test asserts exact equality |
| **B** | Behaviourally equivalent: same law, different precision or interpolation; audibly indistinguishable | CI test asserts the law within tolerance |
| **C** | Inspired: same musical role, different implementation | code review only |
| **D** | Native LXX: no Anushri counterpart | — |

## Summary

| Block | Class | Evidence |
| --- | --- | --- |
| Glide duration law | **A** | `tests/test_glide.cpp` — measured sweep matches `65536/(inc·control_rate)` exactly at 3 sample rates |
| Firmware lookup tables (4) | **A** | `tests/test_firmware_tables.cpp` — 256/256 entries exact for glide, drum env, env and LFO increments |
| Control rates | **A** | asserted 2450.9375 Hz / 1225.46875 Hz (Python 2 integer division) |
| Envelope time law | **A** table / **B** precision | table exact; 16.16 counter vs 16-bit counter |
| Drum pitch / amp envelopes | **B** | same tables, float32 vs fixed point |
| VCF (SVF) | **B** | same TPT topology and `k`/`r` definitions, float32 vs fixed point |
| VCA (AS2164) | **B** | same model curve, float32 vs fixed point |
| LFO | **A** table / **B** precision | `lut_res_lfo_increments` exact; phase precision differs |
| Oscillator | **B** | 32-bit phase vs 16-bit DCO |
| Oversampling | **D** | no AVR counterpart |
| Techno / extended voices | **D** | native LXX |

## What "bit-exact" covers and what it does not

The four increment tables are the only **A**-class numeric claims, because
they are the only artefacts that can be transcribed and asserted without
hardware in the loop. Everything else is **B**: the *law* is asserted
(durations, curve shapes, bounds) but not the last bit of a float32
computation, because the AVR computed in fixed point and no reference
recording exists to compare against.

## Open fidelity questions

1. **Drum phase increments.** `lut_res_drm_phase_increments` is transcribed
   into `drum_resources` but no test regenerates it from
   `lookup_tables.py`. Either add the generator or document why it cannot be
   regenerated.
2. **LFO waveform tables.** `wav_res_lfo_waveforms` is transcribed; the
   interpolation between entries (linear vs nearest) is not asserted.
3. **VCA curve.** `VcaCurve` is asserted to be monotonic and bounded, not to
   match the AS2164 transfer function sample by sample.
4. **Self-oscillation onset.** `SVFAnushriNonlinear` is asserted to stay
   bounded and finite, not to match the hardware's exact clipping behaviour.
