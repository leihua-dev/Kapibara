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

## AI Sample Generation

The sampler's `AI` button generates a sample from a text prompt. The plugin
contains **no model**: bundling an inference runtime would drag a large native
dependency across four plugin formats and three platforms, and model weights
carry their own licence, which has no business inside a GPL tree. It shells out
to a generator you configure and then loads the WAV it produced through exactly
the path a manual LOAD uses.

Configure `presets/ai-generate.cmd` (written with instructions on first use):

```
kapibara-generate --prompt-file {prompt_file} --out {out} --seconds {seconds}
```

`{prompt_file}` is a UTF-8 file containing the prompt, `{out}` is the `.wav` the
generator must write. **The prompt is never interpolated into the command** —
only paths the plugin generated are substituted — so no prompt can become shell
syntax. Generation runs off the UI thread and the result is loaded when it
lands.

A local [Stable Audio Open Small](https://huggingface.co/stabilityai/stable-audio-open-small)
wrapper is a reasonable generator to point this at: ~341M parameters, ONNX,
CPU-capable, roughly 10 s of audio in ~7 s. Note its weights are under
Stability's community licence, not this project's — one more reason they stay
outside the tree.

Expect it to be good at textures, atmospheres, percussion and effects, and weak
at clean pitched instrument samples. AI-generated audio has no defined pitch, so
key-tracked playback of a generated texture maps arbitrarily; `FIXED` mode is
usually what you want for that material.

## Router (Architecture) Presets

The SOURCE column carries its own preset bar: the source rack and its wiring
saved apart from the sound, so a layout — two oscillators into a filter, an FM
stack, a layered rack — can be recalled without disturbing the patch's
modulation. Files are `presets/routers/*.krt`.

They use a strict subset of the modern preset format: tracks, per-voice chains,
inserts, source mods, wires, node positions and merge groups, and **none** of
`mslot` / `mchaos` / `mshape` / `mrule` / `mgrp`. Loading goes through the same
reader as a full preset, which applies matrix rules and MOD curves only when the
file actually contains them — so swapping architecture leaves the modulation
alone by construction rather than by a special case.

Caveat worth knowing: a router preset carries each track's parameters, but
oscillator *frame data* is not in the modern section, so meta-oscillator
wavetables come back at their defaults (same limitation as a full preset —
tables travel as `.kwt` files).

## Presets And Wavetables

- Presets are saved by the plugin in the legacy text format; the UI appends a
  `modern` section carrying the multi-track structure (tracks, routing,
  inserts). Example presets live in `presets/`.
- Wavetables use the Kapibara `.kwt` harmonic format (binary `KWT2`; older
  ASCII `KAPIBARA_WT` files remain loadable). WAV import is also supported.
  See `presets/wavetables/README.md`.
- Sample / Noise tracks: Noise mode (six noise types) and File mode (the WAV
  sampler) are both active. Capture mode is still a UI placeholder.

## Licence

Kapibara's source is licensed under the **GNU General Public License v3.0** —
see `LICENSE`.

Content is a separate work and is **not** covered by that licence: preset files
(`.mfpreset`), wavetables (`.kwt` and imported WAVs) and samples carry their own
terms. Distributing a preset or wavetable pack — including a paid one — does not
oblige anyone to release it under the GPL.

`third_party/DPF` is ISC-licensed (upstream DISTRHO), which is GPL-compatible.
The patches in `third_party/dpf-patches/` are derivative of DPF and carry DPF's
licence, not this project's.

Every plugin format Kapibara builds is permissively licensed, so **no format
forces a licence choice on this project** (see `third_party/DPF/LICENSING.md`):
JACK standalone MIT (RtAudio/RtMidi), LV2 ISC, VST3 ISC, CLAP MIT. VST3 is worth
calling out — DPF does not use Steinberg's SDK but its own `travesty` API
definitions, so there is no Steinberg licensing agreement and no GPL
obligation coming from the format. GPLv3 here is a deliberate choice, and a
proprietary build later is not blocked by the framework.

### Attribution

DPF requires attribution regardless of format. Any distributed build must
credit:

- **DPF** — Copyright 2012-2025 Filipe Coelho (falkTX)
- **RtAudio / RtMidi** (JACK standalone) — Copyright 2001-2021 Gary P. Scavone
- **LV2** — Copyright 2006-2020 Steve Harris, David Robillard; 2000-2002
  Richard W. E. Furse, Paul Barton-Davis, Stefan Westerfeld
- **CLAP** — Copyright 2014-2022 Alexandre Bique

Contributions are accepted under GPLv3 with an additional relicensing grant, so
that a commercially licensed build stays possible — see `CONTRIBUTING.md`. This
has to be settled before the first merged patch, not after.
