# Signal Flow

```text
MIDI note
  -> KapibaraPlugin  (DPF MIDI handler)
  -> SynthCore::noteOn()   (voice allocation, snapshot freeze)
  -> RenderSnapshot        (immutable read-only render state)
  -> Voice x N             (wavetable oscillators + unison + per-voice ADSR)
       -> MatrixEngine     (LFO / ENV modulation applied per control-rate block)
       -> Per-Voice Grid   (currently Source filter per track)
  -> Strip Grid            (per-source bus inserts)
  -> MasterEffects         (Seed tone FX: EQ + filter)
  -> Output gain + safety limiter
```

## Note Events

The on-screen keyboard calls `KapibaraPlugin::previewNoteOn()` /
`previewNoteOff()`, which forward to `SynthCore::noteOn()` / `noteOff()`.
External DPF MIDI events are handled in `KapibaraPlugin::run()` and
forwarded to the same lock-free MIDI queue inside `SynthCore`. Panic calls
`SynthCore::allNotesOff()`.

Every note-on freezes the current master envelope plus each Source Track Amp
Envelope onto the newly allocated voice. Later UI changes publish a new
`RenderSnapshot`; currently held voices pick up track ranges and strip state
at control-rate boundaries (every 32 samples).

## Seed Render State

`SynthCore::publishSnapshotNoLock()` expands the current `SeedPatch`
(`engine/SeedPatch.h`) into one immutable `RenderSnapshot`.

- `Partial Bank` tracks contribute their own additive partial bank. The bank can
  morph between multiple amp/phase frames; each frame maps the first 64
  harmonics to the 64 partial lanes.
- `Meta Oscillator` tracks contribute a multi-frame wavetable (baked by
  `dsp/Generators`).
- `Basic Oscillator` and `Sample / Noise` tracks are converted into bounded
  partial render data within the safety budget (max 64 partial lanes,
  16 source tracks).

The flattened wavetable state is read-only during audio rendering.
Partial Bank count, inharmonic/harmonic-shape edits, and frame morph publish
updated render snapshots from the UI/control path while dragging; they do not
bake Meta wavetable table caches in the audio callback.

## Wavetable Pipeline (UI thread only)

```text
WAV import / TIME draw / SPECTRUM edit
  -> FFT analysis + phase alignment  (dsp/Generators)
  -> frame morphing / reorder        (dsp/Generators)
  -> band-limited mip cache bake     (dsp/Generators, 11 mip levels)
  -> publishSnapshotNoLock()
  -> immutable RenderSnapshot        (audio thread reads from here)
```

File I/O, FFT, phase alignment, and frame morphing never run in the audio
callback.

## Modulation

`MatrixEngine` (`engine/MatrixEngine`) evaluates at control rate (every 32
samples):

- 4 global LFOs with asymmetric shape (ξ, ρ, p\_up, p\_down)
- 4 per-voice ENV breakpoint curves (`MatrixEnvParams::points`)
- 16 routing rules mapping sources (LFO, ENV, velocity, key-track, chaos,
  random, per-voice ADSR) to global partial destinations or a stable track ID
- Realtime-safe track destinations include gain, pan, Meta pitch, Morph and Warp
- Weight functions restrict rules to partial frequency bands (low/mid/high μ
  groups)

`Freq` destination interprets rule depth as octaves (`2^depth`), allowing wide
pitch sweeps. `Amp` destination is clamped to a non-negative gain multiplier.

## Source Routing And Strip Grid

The lower UI is split into `SOURCE ROUTER | PER-VOICE GRID | STRIP GRID`.
The source router owns source selection and first-stage merge groups. Merge
groups only sum member source buses; they do not own hidden insert chains.

The per-voice grid currently exposes a small chain of filter nodes. Their
parameters live on each track's per-voice filter chain and are processed in
`Voice` before audio is accumulated into the source bus.

The strip grid owns bus-level insert chains. Existing track inserts are shown
as strip-grid nodes and are processed by `SynthCore::renderStripBuses()` after
voice accumulation. The fixed `Master` node represents the final bus output.

## Output

Seed tone FX (`dsp/MasterEffects`: 3-band EQ + multi-mode filter) process the
rendered voice mix. Global gain and output safety limiting are applied last.
