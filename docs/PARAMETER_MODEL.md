# Parameter Model

```text
SeedPatch  (model/CompositionModel.h)
  -> Source Track Rack
       -> Partial Bank / Meta Oscillator / Basic Oscillator / Sample+Noise
  -> 4 shared Amp ADSR envelopes
  -> 4 Matrix ENV breakpoint curves  (engine/MatrixEngine)
  -> 16 Matrix routing rules         (engine/MatrixEngine)
  -> Operator chain                  (dsp/Operators)
  -> Tone FX                         (dsp/Effects)
```

## SeedPatch

`SeedPatch` (defined in `model/CompositionModel.h`) is the serialisable preset
boundary. It stores:

- Source Track Rack (track list with per-track generator params)
- 4 shared Amp ADSR banks (`ampEnvIndex` per track selects one)
- 4 drawable Matrix ENV breakpoint curves
- 16 Matrix routing rules
- Operator chain (non-destructive spectral edits)
- Tone FX chain (EQ + filter settings)
- Unison / render quality settings

Parameter lock scopes (`ParameterScope::Global / Seed / Note`) control which
changes are applied at note-on versus globally.

## Source Tracks

Track types and their generators:

| Type | Generator | Source |
|------|-----------|--------|
| Partial Bank | independent additive bank up to 64 partials | `dsp/Generators` |
| Meta Oscillator | multi-frame wavetable, up to 512 × 2048-sample frames | `dsp/Generators` |
| Basic Oscillator | sine/triangle/saw/pulse/sub from bounded partial data | `dsp/Generators` |
| Sample / Noise | Noise active; File/Capture are UI placeholders | `dsp/Generators` |

Each track owns: generator params, gain, pan, send, mute/solo, output mode,
and `ampEnvIndex` (0–3) referencing one shared Amp ADSR.

## Amp Envelopes

Four shared Amp ADSR envelopes (attack, decay, sustain, release). Each track
selects one via `ampEnvIndex`. The legacy Seed ADSR remains as a master
compatibility envelope and global release boundary.

Every note-on freezes the selected ADSR onto the allocated voice. If `attack=0`
the voice starts at full level and immediately enters decay; `sustain=0` still
produces an audible decay transient. Note-off enters release from the current
envelope value.

## Matrix Modulation

Defined in `engine/MatrixEngine`:

- **4 LFOs** — asymmetric shape (ξ, ρ, p\_up, p\_down), plus sine/square/triangle/S&H
- **4 Matrix ENVs** — drawable breakpoint curves (`MatrixEnvParams::points`),
  output unipolar 0..1, first and last points locked to the same level
- **16 routing rules** — source × weight × depth → destination, optionally scoped by stable source-track ID
  - Sources: LFO1–4, ENV1–4, velocity, key-track, chaos, random, per-voice ADSR
  - Destinations: frequency, amplitude, phase, track gain/pan, Meta pitch components, Morph and Warp
  - Weight functions restrict rules to partial groups (low μ=0 / mid μ=1 / high μ=2)

Each voice freezes the active Seed's LFO/ENV parameters at note-on and
evaluates them at control rate (every 32 samples) without heap allocation.
Track-scoped routes are restricted to realtime-safe render parameters; frame
count and waveform-content edits remain outside the audio callback.

## Operators

The operator chain (`dsp/Operators`) applies non-destructive spectral
transformations post-generation, before snapshot publication:

| Operator | Effect |
|----------|--------|
| `PartialMask` | range / group filter |
| `AmpScalePerGroup` | per-band gain |
| `FrequencyJitter` | stochastic detuning |
| `SpectralTilt` | brightness roll-off |
| `HarmonicLock` | snap partials to integer ratios |

## Tone FX

`dsp/Effects` processes the rendered voice mix (post voice sum, pre output):

- 3-band EQ (low / mid / high)
- Multi-mode filter: low-pass, high-pass, or band-pass
- Modes: normal / linear / nonlinear
- Per-channel state; resonance, drive, mix controls

## Wavetable Detail

A Meta wavetable has up to 512 ordered 2048-sample frames. Each frame stores
synchronised time-domain samples, FFT spectrum, and harmonic metadata.
Band-limited mip tables (11 levels) are baked separately in `dsp/Generators`
and used at runtime to control aliasing.

WAV import (DPF file browser or X11 file drop) offers: Auto Detect, Fixed 2048
Frames, Single Cycle, Constant Pitch, or Manual Cycle Length, with a
128/256/512 destination-frame limit. Long sources are sampled evenly across the
whole file.

Frame editing on the UI thread: duplicate / delete / reorder, circular phase
alignment, linear midpoint morph, wrapped-phase spectral midpoint morph.
The SPECTRUM editor shows the first 256 bins; runtime playback uses the baked
mip tables, not the editor view.

## Unison

Unison is rendered as real oscillator lanes inside each `Voice`
(`engine/Voice`). Cost scales by `enabled partial slots × unison voices ×
active voices`; it is not an aggregate spread kernel. The UI shows soft engine
budget warnings; the audio renderer clamps to fixed safety limits.
