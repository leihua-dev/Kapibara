# Parameter Model

```text
SeedPatch  (engine/SeedPatch.h)
  -> Source Track Rack (≤12 tracks)
       -> Partial Bank / Meta Oscillator / Basic Oscillator / Sample+Noise
       -> Unison, per-voice filter chain (≤4), source mods (≤3)
       -> Strip insert chain
  -> 4 shared Amp ADSR envelopes     (engine/AdsrEnv)
  -> 8 unified MOD slots             (engine/ModCurve)
  -> 16 Matrix routing rules         (engine/ModMatrix)
  -> Chaos + Shape source params     (engine/ModCurve)
  -> Tone FX                         (dsp/MasterEffects)
```

## SeedPatch

`SeedPatch` (defined in `engine/SeedPatch.h`) is the serialisable preset
boundary. It stores:

- `SourceGenParams generator` — the Source Track Rack (track list with
  per-track generator params, route graph, merge groups)
- Master ADSR (`adsr`) plus 4 shared Amp ADSR banks (`ampEnvParams`;
  `ampEnvIndex` per track selects one)
- 8 unified MOD slot curves (`modSlotParams`)
- 16 Matrix routing rules (`matrixRules`)
- Chaos and Shape source params
- Tone FX chain (master EQ + filter settings)

## Source Tracks

Track types and their generators:

| Type | Generator | Source |
|------|-----------|--------|
| Partial Bank | independent additive bank up to 64 partials with frame morph | `dsp/GeneratorBank` |
| Meta Oscillator | multi-frame wavetable, up to 512 × 2048-sample frames | `dsp/WavetableCore` |
| Basic Oscillator | rack of 3 units (sine/triangle/saw/pulse/sub each), summed, with Ring/AM/Sync/FM/PM between units | `dsp/BasicOscDsp` |
| Sample / Noise | STREAM source: stereo WAV sampler (root note, OCT/SEM/FIN/CRS, key-track, slices, loop, reverse) or six noise types | `dsp/SampleNoiseDsp` |

Each track owns: generator params, gain, pan, send, mute/solo, output mode
(Audio / ModOnly / AudioAndMod), unison params (Basic Oscillator excepted — the
rack is already three oscillators, so unison is forced to one lane and its zone
is not offered), per-voice filter chain
(≤4 nodes), source-mod entries (≤3), strip insert chain, and `ampEnvIndex`
(0–3) referencing one shared Amp ADSR.

Meta Oscillator, Partial Bank and the sampler share one OCT/SEM/FIN/CRS pitch
module (`trackGroupPitch` / `applyTrackGroupPitch` resolve where each type keeps
the offset); Basic Oscillator has one per rack unit instead. Meta warp modes are
None / Bend / Squeeze / Skew. Meta pitch merges into a frequency ratio;

## Amp Envelopes

Four shared Amp ADSR envelopes plus the legacy master ADSR
(`engine/AdsrEnv.h`). Each stage has its own curvature (`curveA`, `curveD`,
`curveR`, edited by Ctrl-drag on the envelope graph). Each track selects one
bank via `ampEnvIndex`.

Every note-on freezes the selected ADSR onto the allocated voice. If `attack=0`
the voice starts at full level and immediately enters decay; `sustain=0` still
produces an audible decay transient. Note-off enters release from the current
envelope value.

## Matrix Modulation

Defined in `engine/ModCurve.h` (sources) and `engine/ModMatrix.h` (rules):

- **8 unified MOD slots** — each a breakpoint curve of up to 16 points with
  per-segment curvature, a rate in Hz, and a `loop` flag. `loop=true` is LFO
  behaviour (continuous phase, note-on retrigger); `loop=false` is a one-shot
  envelope that holds its end value. Output is bipolar −1..+1. The first four
  slots are the legacy "LFO1–4", the second four the legacy "ENV1–4".
- **Chaos source** — white/smooth/crackle noise generator with frequency and
  amount.
- **Shape source** — spectral-domain static modulator
  (asymmetric/sine/square/triangle/S&H over partial index or spectral x).

Chaos and Shape each have their own page in the bottom Modulators panel,
reached by the CHAOS / SHAPE chips in the strip above it. Chaos plots over time
(it is a noise source); Shape plots over its axis (it is a distribution across
simultaneous partials) and the plot calls `ModMatrix::shapeOutput` directly, so
it cannot drift from what is evaluated. Rho/Up/Down only shape the Asymmetric
curve and are not drawn for the others.

`Random` is not a variant of `Shape`: it is one value per VOICE, constant for
that note's lifetime (classic per-note humanisation), whereas `Shape` is one
value per PARTIAL, identical on every note. They vary along different axes.
- **16 routing rules** — source × weight × depth → destination, optionally
  scoped by stable source-track ID (`targetTrackId`) and insert slot
  (`targetSlot`).
  - Sources: MOD slots, velocity, key-track, random, chaos, shape,
    generator-self, per-voice ADSR 1–4
  - Destinations: partial `Amp` / `Freq` / `Phase`, `DecayTime`,
    `SpectralDecay`, track gain/pan, Meta pitch oct/sem/fine/crs,
    Meta morph/warp/pan, and strip-insert params `InsertP0`–`InsertP3`
  - Weight modes: all, low/high partials, μ groups (low/mid/high), band index

Each voice freezes the active Seed's MOD parameters at note-on and evaluates
them at control rate (every 32 samples) without heap allocation. Track-scoped
routes are restricted to realtime-safe render parameters; frame count and
waveform-content edits remain outside the audio callback.

## Strip Inserts And Source Mods

Strip insert kinds (`dsp/InsertEffects.h`, DSP in `dsp/fx/`): filter,
distortion, EQ, compressor, delay, reverb, convolution reverb, multiband.
Four macro parameters per insert (P0–P3) are matrix-modulatable.

The filter's allpass algos (AP2/AP4/AP8) are a **disperser**: a cascade of up
to `kMaxDisperserStages` second-order allpass sections, each with its own
frequency and Q. Magnitude stays flat and only phase moves, so a transient is
smeared into a descending chirp. Four extra controls appear for these algos and
only these algos:

- **Stages** — 1..32 sections. Total delay scales with the count.
- **Spread** — walks the section frequencies across ±2 octaves around Cutoff.
  This is what makes it a disperser rather than a plain phase shift: with
  Spread at 0 every section is identical and the behaviour reduces exactly to
  the historic cascade.
- **Pinch** — the same idea applied to section Q.
- **Curve** — bends the progression across the cascade (same bend family as the
  modulation curves), which decides *which* bands lag.

`apStages = 0` means "use the count the algo implies" (AP2/AP4/AP8 → 1/2/4), so
presets written before these controls existed sound exactly as they did.

Source mods (`SourceModEntry`): a track can be modulated by another track via
AM, RingMod, FM, PM, or hard sync, with a depth control, rendered per voice.

## Tone FX

`dsp/MasterEffects` processes the rendered bus mix after strip inserts:

- 3-band EQ (low / mid / high)
- Multi-mode filter: low-pass, high-pass, or band-pass
- Modes: normal / linear / nonlinear
- Per-channel state; resonance, drive, mix controls

## Wavetable Detail

A Meta wavetable has up to 512 ordered 2048-sample frames. Each frame stores
synchronised time-domain samples, FFT spectrum, and harmonic metadata.
Band-limited mip tables (11 levels) are baked separately in
`dsp/WavetableCore` and used at runtime to control aliasing.

WAV import (DPF file browser or X11 file drop) offers: Auto Detect, Fixed 2048
Frames, Single Cycle, Constant Pitch, or Manual Cycle Length, with a
128/256/512 destination-frame limit. Long sources are sampled evenly across the
whole file.

Frame editing on the UI thread: duplicate / delete / reorder, circular phase
alignment, linear midpoint morph, wrapped-phase spectral midpoint morph.
The SPECTRUM editor shows the first 256 bins; runtime playback uses the baked
mip tables, not the editor view.

Partial Bank also has a frame table. Each frame stores amp/phase for the
first 64 harmonic partials, reusing the same harmonic `.kwt` wavetable format
as Meta wavetable presets. Loading a Meta `.kwt` into Partial Bank imports only
those first 64 harmonics. `Partials`, `Inharmonic`, frame `Morph`, and frame
selection are runtime render-state changes; Voice smooths per-partial
amplitude/frequency updates across the control block.

New `.kwt` files are saved as binary `KWT2`: frame/bin counts followed by packed
16-bit amplitude and phase bins. The loader still accepts the older ASCII
`KAPIBARA_WT` format for existing presets.

## Persistence

State travels two ways over the same bytes. `getState`/`setState` (DPF
`WANT_STATE` + `WANT_FULL_STATE`) carry the patch in the host session: the
plugin writes the engine half with `writePresetTo` and appends the UI half,
which the UI keeps current by pushing `modernStateString()` on a throttle.
`.mfpreset` files use the identical layout, so one writer and one reader serve
both. Unknown tokens are skipped rather than aborting the load — an older build
has to survive tokens a newer one writes, and the UI appends its own section to
the same file.

The plugin saves the legacy text preset; the UI appends a `modern` section
(`ui/sections/presets/KapibaraUIPresetState.cpp`) with the multi-track
structure: track params/names, per-voice filters, inserts, source mods, route
graph, merge groups, matrix rules (`mrule`), and mask groups (`mgrp` + `mgt`).
Oscillator **content** is in the modern section too, per track: `mmeta` +
`mmetaf` for a Meta oscillator, `mpb` + `mpbf` + `mpbp` + `mpbpf` for a Partial
Bank. It has to be. The legacy `generator` / `bankframes` / `bankframe` /
`partial` / `frame` keys describe ONE global `wavetableSeed`, which only ever
reaches a track through `ensureSourceTracksNoLock()` — and that returns early
the moment a track list exists, so with a modern section present it never runs.
Since the parsed tracks are default-constructed apart from their `mtrack`
scalars, assigning them over the live list destroyed every wavetable in the
patch. Tracks the file says nothing about now inherit their tables from memory,
matched by track id, so presets written before these tokens still load intact.

Frames are stored as base64 of KWT2-packed bins — the same 16-bit amp/phase
packing `.kwt` files use — with trailing silent bins trimmed, so a table using
64 of its 1024 harmonic slots costs 64 bins rather than 1024. A silent frame
writes no line at all. A payload that fails to decode leaves the frame at its
default instead of half-filling it. Router presets skip all of this: a `.krt` is
architecture, and a wavetable is the largest thing in a patch.

Lines grow by appending optional fields at the END, each read with its own
`if(!(ss >> x)) x = <default>;`. This is not stylistic: since C++11 a failed
`operator>>` extraction *writes 0* into its target and leaves the stream in
fail state, so a pre-initialized default is silently destroyed and every later
field on the line fails too. `mgrp`'s tail is, in order: `enabled`,
`waveSource`, `waveTrackId`, `rateHz`. Basic Oscillator racks add `mbosc`
(one line per unit) and `mbmod` (the cross-unit modulation); Sample/Noise adds
`msmp` + `msmpf`. The 8 MOD slots (`mslot`/`mslotp`), Chaos (`mchaos`) and Shape
(`mshape`) are in the modern section too — they were persisted nowhere at all
before, so every rule and mask group came back referencing a default curve.

A preset with **no** `modern` section carries no router at all, and the loader
must not fall through keeping the previous patch's graph — that left wires
pointing into components the new patch does not have, holding output ports that
nothing draws, so the components that did load could not be wired until some
unrelated edit rewrote the leftovers. `resetRouterGraphToDefaultChains()` clears
the graph and lays down what the pre-router engine ran: each source through its
own insert chain into MASTER. Per-voice filter nodes join that default only when
there is a single source — they are shared across the rack and the linear walk
only leaves one by output port 1, so they cannot fan out per track.

An `mwire` line stores the two port refs and nothing else — a wire's drawn path
is recomputed from live port positions every frame, and `GridWire::points` is
only a fallback for ports that are not currently on screen plus storage for a
hand-routed path. The board must therefore render a wire whose `points` is
**empty**, which is exactly the state every preset-loaded wire is in. Skipping
those left them invisible, unselectable and undeletable while they still held
their source's single output port and still matched the duplicate-wire test, so
redrawing the connection by hand was silently swallowed.

The router's *layout* is a separate concern from its wiring. `mwire` carries the
top-level connections and `mnp` / `msw` the graph inside a focused node, but
where the nodes sit on the main board rides `mrnp` (node id, x, y). All of it is
replaced wholesale on load, never merged: the position map used to survive a
preset change, so every node id that collided kept the previous patch's
coordinates while the rest fell back to auto-layout. Merge groups are `msgn`
(index + name, name last on the line so it may hold spaces) plus one `msgm` per
member; members are track *indices*, so ones past the restored track list are
dropped and any group left with fewer than two members goes with them. `maes`
records which amp env each route-node instance drives, written only for assigned
slots (255 = unassigned, which is the fill state). `mrnp`, `msgn`, `msgm` and
`maes` all sit in the structure tail, so a router `.krt` and a full `.mfpreset`
carry them alike.

A mask group whose base is a wavetable stores only the *track id* of the table
owner. Since per-track frames are not written to the modern section, such a
group reconnects to whatever table that track holds after load.

## Unison

Unison is rendered as real oscillator lanes inside each `Voice`
(`engine/Voice`). Cost scales by `enabled partial slots × unison voices ×
active voices`; it is not an aggregate spread kernel. The UI shows soft engine
budget warnings; the audio renderer clamps to fixed safety limits.
