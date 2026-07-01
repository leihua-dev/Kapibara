# Kapibara Architecture

```text
DPF standalone
  -> KapibaraPlugin  (MIDI/audio bridge)
  -> KapibaraUI      (NanoVG / OpenGL3)
  -> SynthCore             (orchestrator + snapshot)
       -> MatrixEngine     (LFO / ENV / matrix rules)
       -> Voice x16        (wavetable oscillators + unison)
       -> Strip Grid       (per-source bus inserts)
       -> MasterEffects    (tone FX)
  -> Output
```

## Directory Structure

```
src/
├── dsp/            signal processing algorithms
│   ├── SpectralFrame.h       StaticSpectralFrame, SpectralTimeline,
│   │                         architectural constants
│   ├── Generators.h/.cpp     wavetable engine: FFT synthesis, WAV import,
│   │                         mip-level bake, frame interpolation, spectral morph
│   ├── InsertEffects.h       strip-grid insert parameters and DSP helpers
│   ├── RouteGraph.h          lightweight route graph node/wire types
│   └── MasterEffects.h/.cpp  3-band EQ, multi-mode filter (LP/HP/BP),
│                             per-channel state, drive/resonance/mix
├── engine/         runtime audio engine
│   ├── MatrixEngine.h/.cpp   4 LFOs (asymmetric/sine/square/tri/S&H),
│   │                         4 drawable ENV breakpoint curves,
│   │                         16 routing rules, velocity/key-track/chaos sources
│   ├── Voice.h/.cpp          polyphonic voice: phase accumulators,
│   │                         wavetable playback, true unison spreading,
│   │                         per-voice ADSR, control-rate interpolation
│   └── SynthCore.h/.cpp      orchestrator: 16-voice pool, MIDI event queue,
│                             RenderSnapshot, undo stack, effects chain
└── plugin/dpf/     DPF plugin framework bridge
    ├── DistrhoPluginInfo.h   DPF metadata, NanoVG/OpenGL3 settings
    ├── KapibaraPlugin.*  DPF synth shell, MIDI→SynthCore bridge,
    │                           audio run(), preset save/load
    └── ui/               NanoVG UI: editor, Source Router, Per-Voice Grid,
                                Strip Grid, Matrix area, preset menu, keyboard
```

## Plugin Shell

`KapibaraPlugin` owns one `SynthCore` instance. It maps incoming DPF MIDI note events to `SynthCore::noteOn()` / `noteOff()`, exposes UI update methods for parameter changes, and calls `SynthCore::renderBlock()` from DPF `run()`.

`KapibaraUI` is a NanoVG single-screen UI. It draws: toolbar and preset menu,
the type-specific editor, Matrix area, bottom Source Router / Per-Voice Grid /
Strip Grid, Panic/status controls, and the bottom keyboard.

## Seed Model

`SeedPatch` is the current sound-state boundary, defined in `engine/SeedPatch.h`. Track types are `Partial Bank`, `Meta Oscillator`, `Basic Oscillator`, and `Sample / Noise`. Each track owns sound data plus gain, pan, send, mute/solo, output mode, strip-grid inserts, and a reference to one of four shared Amp ADSR entries. The four drawable Matrix ENV sources form a separate modulation bank.

UI edits reach `SynthCore` through `KapibaraPlugin` on the UI thread. Audio rendering reads immutable `RenderSnapshot` objects published by `SynthCore::publishSnapshotNoLock()`.

## Audio Core

`SynthCore` publishes one render snapshot for the active Seed. Source Tracks are flattened into a fixed realtime-safe render state with per-track partial ranges and envelopes.

Audio rendering (in `SynthCore::renderBlock()`):
1. Drain pending MIDI note events from the lock-free queue.
2. Render active voices (`Voice::render()`), each reading the current snapshot.
3. Apply per-track per-voice filter state inside `Voice`.
4. Accumulate voices into source buses, then apply strip-grid inserts.
5. Apply Seed tone FX (`MasterEffects`).
6. Apply output gain and safety limiting.

`Mod Only` tracks are excluded from the main audio sum.

## Wavetable Pipeline

Meta wavetable editing runs on the UI thread:

```text
WAV import / TIME draw / SPECTRUM edit
  -> synchronized master frame (FFT analysis, phase alignment)
  -> band-limited mip cache (11 levels, baked in Generators)
  -> immutable RenderSnapshot
```

None of these steps run inside the audio callback.

## Persistence

DPF preset save/load lives in `KapibaraPlugin`. Legacy v4 preset format is readable through migration into Source Tracks; the current in-memory model is track-first. Preset v6 adds Partial Bank frame tables (`bankframes` / `bankframe`) so the additive 64-partial amp/phase morph state is persistent.
