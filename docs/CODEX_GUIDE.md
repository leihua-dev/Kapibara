# Codex Guide

## Must Read

- `AGENTS.md`
- `README.md`
- `docs/ARCHITECTURE.md`
- `docs/SIGNAL_FLOW.md`
- `docs/PARAMETER_MODEL.md`

## Build

```bash
make -C src/plugin/dpf jack
```

Or through the CMake wrapper:

```bash
cmake --build build --config Release
```

Executable:

```text
build/dpf/bin/kapibara
```

## Hotspots

| File | Role |
|------|------|
| `src/plugin/dpf/DistrhoPluginInfo.h` | DPF metadata, NanoVG/OpenGL3 UI setting |
| `src/plugin/dpf/KapibaraPlugin.*` | DPF synth shell, MIDI bridge, audio `run()`, preset save/load |
| `src/plugin/dpf/KapibaraUI.cpp` | NanoVG single-screen Seed dashboard and bottom keyboard |
| `src/engine/SynthCore.h/.cpp` | Seed snapshot publication, voice pool, MIDI queue, undo stack |
| `src/engine/Voice.h/.cpp` | Wavetable oscillator lanes, true unison, per-voice ADSR |
| `src/engine/MatrixEngine.h/.cpp` | 4 LFOs, 4 ENV breakpoint curves, 16 matrix rules, chaos/key-track sources |
| `src/dsp/Generators.h/.cpp` | 64-slot wavetable partial model, FFT, mip-level bake, spectral morph |
| `src/dsp/Operators.h/.cpp` | Non-destructive spectral edit operators applied post-generation |
| `src/dsp/Effects.h/.cpp` | Seed tone FX (EQ + filter) applied after voice mix |
| `src/model/SpectralFrame.h` | Core data type: StaticSpectralFrame (ν, amp, x, μ), SpectralTimeline |
| `src/model/CompositionModel.h` | SeedPatch preset schema, parameter lock scopes |

## Rules

- Keep audio `run()` free of file I/O, JSON, UI work, and large allocation.
- UI-side edits may call the plugin through DPF direct access but must not move wavetable bake or file import into audio processing.
- Do not reintroduce workstation, arrangement, sample lane, mix/export, or framework UI concepts unless explicitly requested.
- After any behaviour change run `make -C src/plugin/dpf jack` to verify the build.
- Update docs when changing signal flow, preset schema, parameter routing, or module boundaries.
