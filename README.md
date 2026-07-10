# Kapibara

Kapibara is a DPF standalone additive / wavetable synthesizer with a
self-drawn NanoVG UI.

```text
DPF JACK standalone
  -> NanoVG self-drawn UI (OpenGL3, fixed 11:7 letterboxed layout)
  -> SynthCore DSP
       -> Voice x16        (additive partial lanes, wavetable playback, unison)
       -> ModMatrix        (8 unified MOD slots + 16 routing rules)
       -> Per-voice chain  (up to 4 filters per track, inside Voice)
       -> Strip buses      (per-source insert FX racks, merge groups)
       -> MasterEffects    (tone FX: 3-band EQ + multi-mode filter)
```

## Source Layout

```
src/
├── dsp/            signal processing algorithms
│   ├── SpectralFrame.h    StaticSpectralFrame / SpectralTimeline + budgets
│   ├── Generators.h       source-track data model, wavetable + render types
│   ├── WavetableCore.cpp  FFT synthesis, WAV/.kwt import, mip bake, morph
│   ├── GeneratorBank.cpp  track flattening into render state
│   ├── BasicOscDsp.cpp    sine/triangle/saw/pulse/sub partial builders
│   ├── SampleNoiseDsp.cpp noise seed builder
│   ├── InsertEffects.h    strip insert params + track source-mod entries
│   ├── InsertChain.h      insert chain processing
│   ├── RouteGraph.h       route graph node/wire types
│   ├── MasterEffects.*    tone FX (3-band EQ + LP/HP/BP filter)
│   └── fx/                per-insert DSP: filter, distortion, EQ, compressor,
│                          delay, reverb, convolution, (multiband via EqFx)
├── engine/         runtime audio engine
│   ├── SynthCore.*        orchestrator: RenderSnapshot, voice pool, MIDI
│   │                      queue, strip buses, undo stack
│   ├── Voice.*            per-voice render: partial lanes, true unison,
│   │                      per-voice filters, source mods (AM/RM/FM/PM/sync)
│   ├── ModMatrix.*        matrix rules + per-voice modulation evaluation
│   ├── ModCurve.h         unified MOD slot (loop/one-shot breakpoint curve),
│   │                      chaos + shape sources
│   ├── AdsrEnv.*          ADSR with per-stage curvature
│   ├── SeedPatch.h        serialisable preset boundary
│   └── MatrixEngine.h     legacy umbrella header (includes ModMatrix.h)
└── plugin/dpf/     plugin framework bridge
    ├── DistrhoPluginInfo.h   DPF metadata, NanoVG/OpenGL3 settings
    ├── KapibaraPlugin.*      DPF MIDI/audio bridge, legacy preset save/load
    └── ui/                   NanoVG UI split into sections/ (core, osc,
                              source, router, matrix, fx, menus, input,
                              visuals, page, presets, sync) + state/ headers
```

## Build

```bash
make -C src/plugin/dpf jack
```

CMake wrapper (equivalent):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Run:

```bash
./build/dpf/bin/kapibara
```

Requires JACK, OpenGL 3, and the DPF submodule in `third_party/DPF`.

## Workflow

1. Open the standalone.
2. Add Source Tracks (max 12): Partial Bank, Meta Oscillator, Basic
   Oscillator, or Sample / Noise.
3. Edit the selected track in the top row: source editor, per-voice chain
   (filters), and FX rack (bus inserts).
4. Drag MOD sources from the strip onto knobs, or open the Matrix dashboard
   in the bottom workspace; expand it into the full route board for source
   routing and merge groups.
5. Play from the bottom keyboard or external MIDI routed to the JACK
   standalone. Use Panic to clear held voices.

## Presets And Wavetables

- Presets are saved by the plugin in the legacy text format; the UI appends a
  `modern` section carrying the multi-track structure (tracks, routing,
  inserts). Example presets live in `presets/`.
- Wavetables use the Kapibara `.kwt` harmonic format (binary `KWT2`; older
  ASCII `KAPIBARA_WT` files remain loadable). WAV import is also supported.
  See `presets/wavetables/README.md`.
- Sample / Noise tracks: Noise mode is active; File and Capture modes are UI
  placeholders in this version.
