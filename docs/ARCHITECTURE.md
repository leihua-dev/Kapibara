# MotifForge Seed Architecture

```text
DPF standalone
  -> MotifForgeSeedPlugin  (MIDI/audio bridge)
  -> MotifForgeSeedUI      (NanoVG / OpenGL3)
  -> SynthCore             (orchestrator + snapshot)
       -> MatrixEngine     (LFO / ENV / matrix rules)
       -> Voice x16        (wavetable oscillators + unison)
       -> Effects          (tone FX)
  -> Output
```

## Directory Structure

```
src/
├── model/          pure data — no audio processing
│   ├── SpectralFrame.h       StaticSpectralFrame (ν, amp, x, μ),
│   │                         SpectralTimeline, architectural constants
│   └── CompositionModel.h    SeedPatch (serialisable preset),
│                             parameter lock scopes (Global/Seed/Note)
├── dsp/            signal processing algorithms
│   ├── Generators.h/.cpp     wavetable engine: FFT synthesis, WAV import,
│   │                         mip-level bake, frame interpolation, spectral morph
│   ├── Operators.h/.cpp      non-destructive spectral operators:
│   │                         PartialMask, AmpScalePerGroup, FrequencyJitter,
│   │                         SpectralTilt, HarmonicLock
│   └── Effects.h/.cpp        3-band EQ, multi-mode filter (LP/HP/BP),
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
    ├── MotifForgeSeedPlugin.*  DPF synth shell, MIDI→SynthCore bridge,
    │                           audio run(), preset save/load
    └── MotifForgeSeedUI.cpp    NanoVG top-level UI: Source Rack, strip rack,
                                Matrix area, preset menu, bottom keyboard
```

## Plugin Shell

`MotifForgeSeedPlugin` owns one `SynthCore` instance. It maps incoming DPF MIDI note events to `SynthCore::noteOn()` / `noteOff()`, exposes UI update methods for parameter changes, and calls `SynthCore::renderBlock()` from DPF `run()`.

`MotifForgeSeedUI` is a NanoVG single-screen UI. It draws: toolbar and preset menu, left source-track list, center type-specific editor, right strip rack, bottom Matrix area, Panic/status controls, and the bottom keyboard.

## Seed Model

`SeedPatch` is the current sound-state boundary, defined in `model/CompositionModel.h`. Track types are `Partial Bank`, `Meta Oscillator`, `Basic Oscillator`, and `Sample / Noise`. Each track owns sound data plus gain, pan, send, mute/solo, output mode, and a reference to one of four shared Amp ADSR entries. The four drawable Matrix ENV sources form a separate modulation bank.

UI edits reach `SynthCore` through `MotifForgeSeedPlugin` on the UI thread. Audio rendering reads immutable `RenderSnapshot` objects published by `SynthCore::publishSnapshotNoLock()`.

## Audio Core

`SynthCore` publishes one render snapshot for the active Seed. Source Tracks are flattened into a fixed realtime-safe render state with per-track partial ranges and envelopes.

Audio rendering (in `SynthCore::renderBlock()`):
1. Drain pending MIDI note events from the lock-free queue.
2. Render active voices (`Voice::render()`), each reading the current snapshot.
3. Apply per-track strip/filter state.
4. Apply Seed tone FX (`Effects`).
5. Apply output gain and safety limiting.

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

DPF preset save/load lives in `MotifForgeSeedPlugin`. Legacy v4 preset format is readable through migration into Source Tracks; the current in-memory model is track-first.
