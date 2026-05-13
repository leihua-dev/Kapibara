# Source Module Map

`Main.cpp` currently owns most UI panels and should shrink over time.

Module groups:

- Core/realtime: `SynthCore.*`, `Voice.*`, `SpectralFrame.h`
- Seed generation: `Generators.*`, `FunctionalSampleSource.cpp`, `SamplePlaybackEngine.*`
- Modulation/editing: `MatrixEngine.*`, `Operators.*`
- Processing: `Effects.*`, `ResamplingEngine.*`
- Composition data: `CompositionModel.h`
- UI: `ui/` plus remaining panels in `Main.cpp`

Extraction rule:

- New reusable UI components go into `src/ui/`.
- New DSP modules should avoid including UI headers.
- Data model headers should avoid depending on JUCE GUI classes.
