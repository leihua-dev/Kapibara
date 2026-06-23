# Kapibara Agent Guide

This repository is Kapibara, a DPF standalone wavetable instrument.
Treat the current code as the source of truth: the production app is a single
Seed synth built with DPF, NanoVG, OpenGL3, and the existing `SynthCore` DSP.
It is not a JUCE workstation and does not contain Motif lanes, sample lanes,
mix/export, or preset save/load services.

## Source Layout

```
src/
├── model/       SpectralFrame.h, CompositionModel.h  — pure data structures
├── dsp/         Generators, Operators, Effects        — signal processing
├── engine/      MatrixEngine, Voice, SynthCore        — runtime audio engine
└── plugin/dpf/  KapibaraPlugin.*, UI.cpp       — DPF plugin shell + NanoVG UI
```

## Product Boundary

```
DPF UI  ->  KapibaraPlugin  ->  SynthCore  ->  Voice  ->  Effects  ->  Output
```

## Rules

- Keep audio callback code realtime-safe: no heap allocation, file I/O, locks,
  JSON, logging, or sample analysis in `SynthCore::renderBlock()` or `Voice::render()`.
- UI code may mutate Seed params through `KapibaraPlugin` and `SynthCore`,
  but must not reach into `Voice` internals directly.
- `src/plugin/dpf/` owns the plugin shell and NanoVG UI.
  DSP behaviour lives in `src/engine/`, `src/dsp/`, and `src/model/`.
- One active `SeedPatch` is the current sound object. Do not reintroduce old
  workstation, arrangement, drum pad, or sample lane concepts unless explicitly
  requested.
- After behaviour changes run `make -C src/plugin/dpf jack` and confirm a clean
  build before committing.
- Update docs when changing signal flow, preset schema, parameter routing, or
  module boundaries.
- Do not add new public parameters ad hoc; first identify the DPF/plugin boundary
  that should expose them.
- Avoid moving DSP files in the same patch as behaviour changes.

## Performance Warning

Partial count × unison × active voices is the main oscillator cost.
64 rendered partials at 16 unison voices = 1024 oscillator lanes before FX.
