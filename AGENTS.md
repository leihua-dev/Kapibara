# Kapibara Agent Guide

This repository is Kapibara, a DPF standalone wavetable instrument.
Treat the current code as the source of truth: the production app is a single
Seed synth built with DPF, NanoVG, OpenGL3, and the existing `SynthCore` DSP.
It is not a JUCE workstation and does not contain Motif lanes, sample lanes,
or mix/export services.

## Source Layout

```
src/
├── dsp/         SpectralFrame, Generators/WavetableCore, InsertEffects,
│                MasterEffects, fx/            — data model + signal processing
├── engine/      SynthCore, Voice, ModMatrix, ModCurve, AdsrEnv, SeedPatch
│                                              — runtime audio engine
└── plugin/dpf/  KapibaraPlugin.*, ui/         — DPF plugin shell + NanoVG UI
```

## Product Boundary

```
DPF UI  ->  KapibaraPlugin  ->  SynthCore  ->  Voice  ->  Strip buses  ->  Tone FX  ->  Output
```

## Hotspots

| File | Role |
|------|------|
| `src/plugin/dpf/DistrhoPluginInfo.h` | DPF metadata, NanoVG/OpenGL3 UI setting |
| `src/plugin/dpf/KapibaraPlugin.*` | DPF synth shell, MIDI bridge, audio `run()`, legacy preset save/load |
| `src/plugin/dpf/ui/` | NanoVG UI: `sections/` drawing+input units, `state/` member headers |
| `src/engine/SynthCore.*` | RenderSnapshot publication, voice pool, MIDI queue, strip buses, undo |
| `src/engine/Voice.*` | Partial oscillator lanes, true unison, per-voice filters, source mods |
| `src/engine/ModMatrix.*` | 8 unified MOD slots, 16 matrix rules, chaos/shape/key-track sources |
| `src/engine/ModCurve.h` | `ModSlotParams` breakpoint curve (loop = LFO, one-shot = ENV) |
| `src/dsp/Generators.h` | Source-track data model, render budgets, wavetable types |
| `src/dsp/WavetableCore.cpp` | FFT synthesis, WAV/.kwt import, mip-level bake, spectral morph |
| `src/dsp/InsertEffects.h` | Strip insert parameters, track source-mod entries |
| `src/dsp/SpectralFrame.h` | Architectural constants and the flattened spectral frame |
| `src/engine/SeedPatch.h` | Serialisable SeedPatch preset boundary |

## Rules

- Keep audio callback code realtime-safe: no heap allocation, file I/O, locks,
  JSON, logging, or sample analysis in `SynthCore::renderBlock()` or `Voice::render()`.
- UI code may mutate Seed params through `KapibaraPlugin` and `SynthCore`,
  but must not reach into `Voice` internals directly.
- `src/plugin/dpf/` owns the plugin shell and NanoVG UI.
  DSP behaviour lives in `src/engine/` and `src/dsp/`.
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
- New UI drawing/input code goes into the matching `ui/sections/` unit; new UI
  member state goes into the matching `ui/state/` header.

## Performance Warning

Partial count × unison × active voices is the main oscillator cost.
Budgets (from `SpectralFrame.h` / `Generators.h`): 16 voices, 12 source
tracks, 64 partial slots per track, 500 flattened partial lanes, 16 unison.
64 partials at 16 unison voices = 1024 oscillator lanes per voice before FX.
