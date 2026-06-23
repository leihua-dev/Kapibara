# Kapibara

Kapibara is a DPF standalone additive wavetable synthesizer.

```text
DPF JACK standalone
  -> NanoVG self-drawn UI (OpenGL3)
  -> SynthCore DSP
       -> Voice (polyphonic wavetable oscillators)
       -> MatrixEngine (LFO / ENV modulation)
       -> Effects (tone FX)
```

## Source Layout

```
src/
├── model/          pure data structures
│   ├── SpectralFrame.h       partial freq/amp frame and timeline
│   └── CompositionModel.h    SeedPatch preset, parameter lock scopes
├── dsp/            signal processing algorithms
│   ├── Generators.h/.cpp     wavetable synthesis, FFT, mip-level bake
│   ├── Operators.h/.cpp      non-destructive spectral edit operators
│   └── Effects.h/.cpp        3-band EQ and multi-mode filter
├── engine/         runtime audio engine
│   ├── MatrixEngine.h/.cpp   4 LFOs, 4 ENVs, 16 matrix routing rules
│   ├── Voice.h/.cpp          polyphonic voice, unison, per-voice ADSR
│   └── SynthCore.h/.cpp      orchestrator, snapshot, MIDI queue, undo
└── plugin/dpf/     plugin framework bridge
    ├── DistrhoPluginInfo.h
    ├── KapibaraPlugin.*  DPF MIDI/audio bridge to SynthCore
    └── KapibaraUI.cpp    NanoVG Source Rack UI and keyboard
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

## Workflow

1. Open the standalone.
2. Add/edit Source Tracks: Partial Bank, Meta Oscillator, Basic Oscillator, or Sample / Noise.
3. Adjust each track's strip, Amp Envelope, output mode, and track-specific editor.
4. Play from the bottom keyboard or external MIDI routed to the JACK standalone.
5. Use Panic to clear held voices.

DPF user preset save/load is implemented in the standalone. Sample/File and Capture tracks are UI placeholders in this version; Noise mode is active.
