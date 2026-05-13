# MotifForge Architecture

MotifForge is organized around five music-object layers:

```text
Seed -> Creator -> Motif -> Structure -> MixingArrangeView
```

Layer boundaries:

- `Seed`: sound atom. Owns generator, spectral timeline, operator chain, matrix mapping, voice render, and Tone FX.
- `Creator`: playable sound object. Owns multi-seed organization, key/drum mapping, PianoLocks, SampleCraft, and resampling.
- `Motif`: phrase or pattern-like musical idea. References Creators and owns local note events, locks, and local automation.
- `Structure`: arrangement/form logic. References Motifs and owns sections, placement, development transforms, and global automation.
- `MixingArrangeView`: final engineering view. Owns track/bus preview, mix routing, sends, stem render, and master processing.

Reference rule:

- Higher layers reference lower layers.
- Higher layers should not deep-copy lower-layer state.
- Changes should be represented as overrides, automation, or transforms.
- Audio freeze/resampling is the explicit boundary where state becomes a buffer.

Current source layout:

```text
src/
  SynthCore.*              realtime coordinator and audio callback bridge
  Voice.*                  oscillator/render voice
  SpectralFrame.h          partial/timeline ABI
  Generators.*             generator params, generator bank, functional cache
  FunctionalSampleSource.* sample analysis and functional spectral bake
  MatrixEngine.*           LFO/Shape/Chaos/performance modulation
  Operators.*              non-destructive spectral edit chain
  Effects.*                shared Tone/Mix FX DSP
  ResamplingEngine.*       Creator SampleCraft buffer processing
  SamplePlaybackEngine.*   ordinary sampler path
  CompositionModel.h       Seed/Creator/Motif/Structure/Mixer data model
  ui/                      UI components extracted from Main.cpp
```

`Main.cpp` is still a migration hotspot. New UI components should move into `src/ui/` instead of growing `Main.cpp`.
