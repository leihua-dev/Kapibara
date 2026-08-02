# Kapibara Architecture

```text
DPF standalone
  -> KapibaraPlugin  (MIDI/audio bridge, legacy preset save/load)
  -> KapibaraUI      (NanoVG / OpenGL3, fixed 11:7 letterboxed layout)
  -> SynthCore             (orchestrator + RenderSnapshot)
       -> ModMatrix        (8 unified MOD slots / 16 matrix rules)
       -> Voice x16        (partial lanes + unison + per-voice chain)
       -> Strip buses      (per-source insert racks, merge groups)
       -> MasterEffects    (tone FX)
  -> Output
```

## Directory Structure

```
src/
├── dsp/            data model + signal processing
│   ├── SpectralFrame.h       StaticSpectralFrame, SpectralTimeline,
│   │                         architectural constants (budgets)
│   ├── Generators.h          source-track data model: track params, wavetable
│   │                         types, route graph state, render runtime
│   ├── WavetableCore.cpp     wavetable engine: FFT synthesis, WAV/.kwt import,
│   │                         mip-level bake, frame interpolation, spectral morph
│   ├── GeneratorBank.cpp     flattening tracks into realtime render state
│   ├── BasicOscDsp.cpp       basic-oscillator partial builders
│   ├── SampleNoiseDsp.cpp    noise seed builder
│   ├── InsertEffects.h       strip insert parameters + source-mod entries
│   ├── InsertChain.h         insert chain processing helpers
│   ├── RouteGraph.h          route graph node/wire types
│   ├── fx/                   per-insert DSP (filter, distortion, EQ,
│   │                         compressor, delay, reverb, convolution)
│   └── MasterEffects.h/.cpp  3-band EQ, multi-mode filter (LP/HP/BP),
│                             per-channel state, drive/resonance/mix
├── engine/         runtime audio engine
│   ├── ModCurve.h            unified MOD slot: breakpoint curve (≤16 points,
│   │                         per-segment curvature), loop or one-shot;
│   │                         chaos + shape source params
│   ├── ModMatrix.h/.cpp      16 routing rules, per-voice evaluation,
│   │                         velocity/key-track/chaos/random/ADSR sources
│   ├── MatrixEngine.h        legacy umbrella header → ModMatrix.h
│   ├── AdsrEnv.h/.cpp        ADSR with per-stage curvature (A/D/R)
│   ├── Voice.h/.cpp          polyphonic voice: phase accumulators, wavetable
│   │                         playback, true unison, per-voice filter chain,
│   │                         source mods (AM/RM/FM/PM/hard sync)
│   ├── SeedPatch.h           serialisable preset boundary
│   └── SynthCore.h/.cpp      orchestrator: 16-voice pool, MIDI event queue,
│                             RenderSnapshot, strip buses, undo stack
└── plugin/dpf/     DPF plugin framework bridge
    ├── DistrhoPluginInfo.h   DPF metadata, NanoVG/OpenGL3 settings
    ├── KapibaraPlugin.*      DPF synth shell, MIDI→SynthCore bridge,
    │                         audio run(), legacy preset save/load
    └── ui/                   NanoVG UI
        ├── KapibaraUI.hpp        class definition (members via state/)
        ├── KapibaraUIDrawing.h   drawing primitives (panels, knobs, fonts)
        ├── KapibaraUIShared.h    DesignTokens, enums, small helpers
        ├── sections/             drawing + input, one directory per area:
        │                         core, page, source, osc/(partialbank, meta,
        │                         basic, noise, shared), pervoice, fx, router,
        │                         matrix, menus, presets, input, visuals, sync
        └── state/                member-variable headers included by the class
```

## Plugin Shell

`KapibaraPlugin` owns one `SynthCore` instance. It maps incoming DPF MIDI note
events to `SynthCore::noteOn()` / `noteOff()`, exposes UI update methods for
parameter changes, and calls `SynthCore::renderBlock()` from DPF `run()`.

`KapibaraUI` is a NanoVG single-window UI, letterboxed to a fixed 11:7 aspect
(default 1320×840, minimum 1100×700). Layout:

- Toolbar (preset menu, options, panic).
- Top row: Source editor | Per-Voice Chain editor | FX Rack editor for the
  selected track. Double-clicking a chain node swaps the row for a focused
  detail view; the multiband insert opens a full-width editor.
- MOD-source strip: draggable modulation sources.
- Bottom workspace: Source column plus either the Matrix dashboard
  (collapsed) or the full route board with merge groups (expanded).
- Bottom keyboard.

## Seed Model

`SeedPatch` (`engine/SeedPatch.h`) is the current sound-state boundary. It
holds the generator params (`SourceGenParams` with the track rack), the master
ADSR, 4 shared Amp ADSRs, 8 unified MOD slots, 16 matrix rules, chaos + shape
source params, and tone FX.

Track types are `Partial Bank`, `Meta Oscillator`, `Basic Oscillator`, and
`Sample / Noise`. Each track owns sound data plus gain, pan, send, mute/solo,
unison, per-voice filters, strip inserts, source-mod entries, and a reference
to one of four shared Amp ADSRs.

UI edits reach `SynthCore` through `KapibaraPlugin` on the UI thread. Audio
rendering reads immutable `RenderSnapshot` objects published by
`SynthCore::publishSnapshotNoLock()`.

## Audio Core

`SynthCore` publishes one render snapshot for the active Seed. Source Tracks
are flattened into a fixed realtime-safe render state with per-track partial
ranges and envelopes.

Audio rendering (in `SynthCore::renderBlock()`):
1. Drain pending MIDI note events from the lock-free queue.
2. Render active voices (`Voice::render()`), each reading the current snapshot.
3. Inside `Voice`: evaluate the mod matrix at control rate, apply source mods
   (AM/RM/FM/PM/hard sync between tracks), then the per-track per-voice
   filter chain.
4. Accumulate voices into source buses, then apply strip insert chains
   (`renderStripBuses()`), merge groups, and the master bus.
5. Apply Seed tone FX (`MasterEffects`).
6. Apply output gain and safety limiting.

`Mod Only` tracks are excluded from the main audio sum.

## Wavetable Pipeline

Meta wavetable editing runs on the UI thread:

```text
WAV / .kwt import, TIME draw, SPECTRUM edit
  -> synchronized master frame (FFT analysis, phase alignment)
  -> band-limited mip cache (11 levels, baked in WavetableCore)
  -> immutable RenderSnapshot
```

None of these steps run inside the audio callback.

## Persistence

Legacy preset save/load lives in `KapibaraPlugin`. The UI appends a `modern`
section to the same file (`saveModernState()` in
`ui/sections/presets/KapibaraUIPresetState.cpp`) carrying the multi-track
structure: tracks, per-voice filters, inserts, route graph, merge groups.
Oscillator content (Meta and Partial Bank wavetables) is carried per track as
base64 KWT2 payloads; see PARAMETER_MODEL.md. The legacy `bankframes` keys hold
only the single global seed and are not a per-track store.

Wavetables are saved as binary `KWT2` `.kwt` files (frame/bin counts followed
by packed 16-bit amplitude and phase bins); the loader still accepts the older
ASCII `KAPIBARA_WT` format.
