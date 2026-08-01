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
git clone --recurse-submodules <repo>
cd kapibara
./scripts/setup-deps.sh      # fetches DPF and applies the patches below
make -C src/plugin/dpf       # standalone + LV2 + VST3 + CLAP
```

`setup-deps.sh` is not optional and is not just `submodule update`. Kapibara
builds against a **patched DPF**: upstream has no `uiClipboardData` hook, and
pugl's X11 backend does not deliver file drops — the UI needs both. The patches
live in `third_party/dpf-patches/` and the script applies them idempotently, so
re-running it on an already-patched tree is a no-op. Without it the build fails
at `uiClipboardData ... does not override`.

Build one format only:

```bash
make -C src/plugin/dpf jack     # or lv2 / vst3 / clap
./build/dpf/bin/kapibara
```

Outputs land in `build/dpf/bin/`: `kapibara` (JACK standalone),
`kapibara.lv2/`, `kapibara.vst3/`, `kapibara.clap`.

Requires a C++17 compiler, OpenGL 3, and JACK for the standalone.

### CI

The three-platform build workflow lives at `ci/github-workflow-build.yml`. It is
not under `.github/workflows/` in this repo because the token used to push here
lacks GitHub's `workflow` scope; enable it with:

```bash
mkdir -p .github/workflows
git mv ci/github-workflow-build.yml .github/workflows/build.yml
git commit -m "enable CI" && git push
```

It builds every format on Linux, macOS and Windows (MinGW) on each push, and
syntax-checks the engine as C++20 separately (it has no DPF dependency). The
tree has only ever been compiled on Linux — read the first macOS and Windows
runs as findings, not as noise.

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
