# Signal Flow

```text
MIDI note
  -> KapibaraPlugin  (DPF MIDI handler)
  -> SynthCore::noteOn()   (voice allocation, snapshot freeze)
  -> RenderSnapshot        (immutable read-only render state)
  -> Voice x N             (partial lanes + unison + per-voice ADSR)
       -> ModMatrix        (MOD slots evaluated per control-rate block)
       -> Source mods      (AM / RingMod / FM / PM / hard sync between tracks)
       -> Per-voice chain  (up to 4 filters per track)
  -> Strip buses           (per-source insert chains, merge groups, master)
  -> MasterEffects         (Seed tone FX: EQ + filter)
  -> Output gain + safety limiter
```

## Note Events

The on-screen keyboard calls `KapibaraPlugin::previewNoteOn()` /
`previewNoteOff()`, which forward to `SynthCore::noteOn()` / `noteOff()`.
External DPF MIDI events are handled in `KapibaraPlugin::run()` and
forwarded to the same lock-free MIDI queue inside `SynthCore`. Panic calls
`SynthCore::allNotesOff()`.

Beyond note on/off the queue carries pitch bend (14-bit, ±2 semitones, applied
as a per-control-block ratio so held notes follow the wheel rather than being
frozen at note-on), sustain pedal (CC64 — notes released while it is down are
held and released together when it comes up), mod wheel (CC1) and channel
pressure. The last two reach the matrix as `ModSource::ModWheel` and
`ModSource::Pressure`.

Every note-on freezes the current master envelope plus each Source Track Amp
Envelope onto the newly allocated voice. Later UI changes publish a new
`RenderSnapshot`; currently held voices pick up track ranges and strip state
at control-rate boundaries (every 32 samples).

## Seed Render State

`SynthCore::publishSnapshotNoLock()` expands the current `SeedPatch`
(`engine/SeedPatch.h`) into one immutable `RenderSnapshot`.

- `Partial Bank` tracks contribute their own additive partial bank (up to 64
  partials). The bank can morph between multiple amp/phase frames; each frame
  maps the first 64 harmonics to the 64 partial lanes.
- `Meta Oscillator` tracks contribute a multi-frame wavetable (baked by
  `dsp/WavetableCore`).
- `Basic Oscillator` tracks are converted into bounded partial render data
  within the safety budget (12 source tracks, 64 partial slots per track, 500
  flattened partial lanes).
- `Sample / Noise` tracks are a **stream source**: they are not summed out of
  the partial pool at all but rendered as audio in `Voice::renderStreamTrack`
  (WAV playback, or coloured noise). They still claim one silent partial slot
  so that anything keyed off a track's partial range — matrix rules scoped to
  the track, route ranges — keeps resolving. The one thing a stream source has
  to do itself is multiply in `trackEnvScratch_[track][s]`, the same per-sample
  amp envelope the partial renderer applies inside its own loop; everything
  downstream (per-voice filters, strip gain/pan, inserts, buses, modulator
  taps) operates on the track buffer and needs no change.

The flattened wavetable state is read-only during audio rendering.
Partial Bank count, inharmonic/harmonic-shape edits, and frame morph publish
updated render snapshots from the UI/control path while dragging; they do not
bake Meta wavetable table caches in the audio callback.

While a voice is playing a track, `SynthCore` publishes that track's live
(modulated) morph for the UI (`getLiveTrackMorph()`); when no voice is active
it returns a negative sentinel and the UI falls back to the knob value.

## Wavetable Pipeline (UI thread only)

```text
WAV / .kwt import, TIME draw, SPECTRUM edit
  -> FFT analysis + phase alignment  (dsp/WavetableCore)
  -> frame morphing / reorder        (dsp/WavetableCore)
  -> band-limited mip cache bake     (dsp/WavetableCore, 11 mip levels)
  -> publishSnapshotNoLock()
  -> immutable RenderSnapshot        (audio thread reads from here)
```

File I/O, FFT, phase alignment, and frame morphing never run in the audio
callback.

## Modulation

`ModMatrix` (`engine/ModMatrix`) evaluates at control rate (every 32 samples):

- **8 unified MOD slots** (`ModSlotParams` in `engine/ModCurve.h`): each slot
  is a breakpoint curve (up to 16 points, per-segment curvature) with a rate
  and a loop flag — `loop=true` behaves as an LFO (continuous phase,
  retriggered at note-on), `loop=false` as a one-shot envelope that holds its
  final value. Output is bipolar −1..+1.
- **32 routing rules** mapping sources to destinations, optionally scoped to a
  stable track ID.
  - Sources: MOD slots 1–8 (legacy LFO1–4 / ENV1–4 aliases), velocity,
    key-track, random, chaos, shape, generator-self, per-voice ADSR 1–4.
  - Destinations: partial amp/freq/phase, decay time, spectral decay, track
    gain/pan, Meta pitch (oct/sem/fine/crs), Meta morph/warp/pan, and strip
    insert parameters P0–P3.
  - Weight modes restrict rules to partial ranges: all, low/high partials,
    μ groups (low/mid/high), or an explicit band index range.

`Freq` destination interprets rule depth as octaves (`2^depth`), allowing wide
pitch sweeps. `Amp` destination is clamped to a non-negative gain multiplier.

### Mask Groups (advanced tier)

A rule is one source driving one destination. A **mask group** (`MaskGroup`,
4 of them) is one base shape fanned into many lanes, each lane driving its own
target — the way to modulate a whole set of simultaneous things with one
drawn shape.

- **The group owns its rate.** `MaskGroup::rateHz` drives the fan, not the base
  MOD slot's rate: the shape source can change (curve slot, wavetable) without
  the fan changing speed, and unlike `ModSlotParams` it is persisted.
- **Lanes are real.** Each lane owns a phase accumulator advanced at its own
  rate every control block, wrapped mod 1 individually. `freqSpread` scales
  lane rate (up to 2×) across the fan, `phaseSpread` offsets lane phase (±1
  cycle), and `spreadCurve` bends the progression (linear at 0). Because the
  accumulators are real, editing any spread only changes future rates — no
  lane ever jumps, however long the group has been running. Two things keep the
  fan coherent: a lane-count change resamples the old fan's phases onto the new
  lane grid, and zero rate spread pulls every lane back onto lane 0, so "no
  spread" always means one LFO no matter what the fan did earlier.
- **The lane count follows the targets.** 16 for the discrete target slots;
  for a per-partial family it is the track's *resolved* partial range
  (`trackEnd - trackBegin`), so a 64-partial bank really gets 64 independent
  lanes. Families wider than `kMaskFanLanes` crossfade between neighbours.
  Resolved on the parameter thread in `SynthCore::rebuildMaskWaveBankNoLock`.
- **The base shape is a MOD curve or a wavetable.** With a wavetable, lane *k*
  morphs to the frame sitting at its own bent fan position — one layer of the
  table per lane. Frames are decimated to 128-entry bipolar LUTs by `bakeModWaveLut` on the
  parameter thread and normalized by a single table-wide gain, so the
  frame-to-frame amplitude contour survives into the fan.
- The baked LUTs travel in `RenderSnapshot::maskWaves` as a shared pointer
  rebuilt only when a lane count or a table identity changes; the audio thread
  handoff is a pointer compare.

## Source Mods And Per-Voice Chain

Each track carries up to 3 source-mod entries (`SourceModEntry`): another
track modulates it via AM, RingMod, FM, PM, or hard sync, rendered inside
`Voice` before bus accumulation.

A Basic Oscillator track additionally has modulation *inside* itself
(`BasicOscModParams`): one unit of its oscillator rack drives another via
Ring, AM, Sync, FM, or PM (picked from a menu on the mode chip, not by
cycling). This is not a matrix route — the wiring never leaves the source —
but its depth is `ModDestination::OscModDepth`, so an LFO can still sweep it.
A unit's on/off switch controls whether it is HEARD, not whether it exists:
the unit selected as the modulation source is rendered either way, and simply
isn't added to the rack's sum when switched off. The units are contiguous slices of the track's partial block
(`WavetableSeedRenderState::unitBegin/unitEnd`), so `Voice::renderTrackPartials`
renders the modulator and carrier into their own buffers and the untouched
units straight to the output. Every partial is rendered exactly once per
block: the phase accumulators advance during rendering, so covering one
twice would run it at double speed. The per-voice chain adds up to 4 filter
nodes per track, also processed in `Voice`.

## Source Routing And Strip Buses

The bottom workspace holds the Source column and, when expanded, the full
route board. The source column owns source selection; the route board owns
source ordering, first-stage merge groups, and the bus wiring compiled by
`ui/sections/router/KapibaraUIRouteCompile.cpp` into the route-graph render
state. Merge groups only sum member source buses; they do not own hidden
insert chains.

The strip buses own bus-level insert chains (filter, distortion, EQ,
compressor, delay, reverb, convolution reverb, multiband). Track inserts are
processed by `SynthCore::renderStripBuses()` after voice accumulation. The
fixed `Master` node represents the final bus output.

Node ownership differs by kind, and the compile walk provisions on demand rather
than truncating. A strip insert belongs to exactly one track, so a chain that
crosses into another track's insert *adopts* it (`adoptStripInsert`, which
rewrites the node id everywhere it appears). A per-voice filter node is the
opposite: the node is global (`perVoiceNodeId` ignores the track id) while the
params and the slot count are per track, so a chain entering a slot the track
has not provisioned grows that track's `perVoiceFilterCount` and seeds the
params from a track that already owns the slot (`ensurePerVoiceFilterSlot`).
Both exist for the same reason — the board draws filter nodes up to the *global*
max slot count, so a track sitting below it (added after the filter, or restored
from a preset whose tracks disagree) would otherwise show a wire that hit-tests,
draws, and silently carries no signal.

## Filter Slots

A filter insert is a chain of up to `kMaxDisperserStages` slots, not one filter
repeated. `disperserStage()` places each slot (octaves off the cutoff, Q as a
multiple of the knob) and `apAlgo` / `apDist` / `apDrive` / `apFb` voice it, all
using 0 to mean "as before" so a zero-init tail is the previous behaviour exactly.

Two rules are not negotiable, both measured:

- **Feedback forces saturation.** An allpass loop is unconditionally stable —
  `|H| == 1`, so the loop gain is exactly the feedback amount — but a resonant
  slot has `|H| >> 1` at its peak. A 2-pole lowpass at Q 10 with 0.99 feedback
  and no saturator ran away to 2.1e9 with a DC offset of -179000; with the
  saturator it peaks at 1.5 at DC 0.0002. The saturator is applied unconditionally
  to the fed-back sample, never as a user option.
- **Asymmetric distortion needs a DC blocker.** Feedback alone is clean (0.97
  feedback measured DC -0.00002). Tube and Diode are asymmetric by design — that
  is their even-harmonic character — and leave offsets of 0.48 and 0.86. Over a
  chain that offset biases every shaper downstream, so each distorting slot runs
  a ~5 Hz blocker: harmonics kept, offset gone.

Cost is per strip bus, not per voice, which is the only reason this fits: 32
slots with feedback and distortion on every one measured 3.8% of a core, against
0.03% for the single-slot filter it generalises. Slots with neither pay nothing —
both are branch-guarded, and a wholly unvoiced chain takes the original path.

`cleanupRouteGraphForCurrentTracks()` runs once per board frame and drops wires
whose endpoints no longer resolve. Strip nodes are exempt from the *source
deleted* case on purpose (deleting one source must not disconnect another
track's downstream chain), but NOT from a vanished insert: nothing draws such a
node, so the wire is invisible while it still holds its upstream output port,
still feeds the cycle check, and still counts toward `componentOutPortCount()` —
which stacks phantom output dots and shifts every real one off the pixel being
clicked. MASTER is excluded first, since it satisfies `isStripNode` and would
otherwise resolve as a vanished insert and take every wire to the output with
it.

## Output

Seed tone FX (`dsp/MasterEffects`: 3-band EQ + multi-mode filter) process the
rendered bus mix. Global gain and output safety limiting are applied last.
