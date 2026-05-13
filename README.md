# JUCE Additive Synth (PC Replica)

This folder contains a JUCE desktop app that replicates the additive synth engine from the embedded Daisy/RP2040 project.

## What is replicated

- Additive spectrum generation from `common/additive_spectrum_engine.c`
- Same panel parameter ranges and mapping logic
- Frozen random refresh behavior
- Stereo output with normalization and soft clip

## Requirements

- CMake 3.20+
- A C++17 compiler
- Internet access for first configure step (JUCE is fetched from GitHub)

## Build and run (Windows)

```powershell
cd juce_additive_synth
cmake -B build -S . -DFETCHCONTENT_SOURCE_DIR_JUCE="%cd%/third_party/JUCE"
cmake --build build --config Release
.\build\JuceAdditiveSynth_artefacts\Release\Juce Additive Synth.exe
```

If your generator is Ninja (single-config), run:

```powershell
.\build\JuceAdditiveSynth_artefacts\Juce Additive Synth.exe
```

## Notes

- This is a standalone desktop synth app, not a plugin.
- Hardware-specific RP2040 modules (LCD, encoder, UART transport) are intentionally not included.
- JUCE source is vendored in `third_party/JUCE` (shallow clone).
