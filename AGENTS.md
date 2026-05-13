# MotifForge Agent Guide

This repository is MotifForge, a JUCE spectral sound design workstation.

Rules for AI/code agents:

- Keep audio callback code realtime-safe: no heap allocation, file I/O, locks, JSON, logging, or sample analysis in audio render paths.
- UI code may mutate high-level params through `SynthCore`, but should not reach into voice internals.
- Preserve the layer boundaries: Seed produces sound, Creator organizes playable sound objects, Motif organizes phrases, Structure organizes form, MixingArrangeView handles mix/export.
- Prefer small patches and run `cmake --build build --config Release` after behavior changes.
- Update docs when changing signal flow, preset schema, parameter routing, or module boundaries.
- Do not add new public parameters ad hoc; route future broad parameter work through a ParameterManager.
- Avoid moving many DSP files in the same patch as behavior changes.

Performance warning:

- Partial count × unison × active voices is the main oscillator cost. A single 182-partial note at 16 unison voices means 2912 oscillator lanes before FX/resampling.
