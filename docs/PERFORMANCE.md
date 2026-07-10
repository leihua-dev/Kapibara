# Performance Notes

The main DSP cost is additive oscillator lane count:

```text
rendered partial slots  ×  unison voices  ×  active voices
```

The Source Rack UI allows free track creation, but the audio renderer clamps
the flattened realtime budget (constants in `dsp/SpectralFrame.h` and
`dsp/Generators.h`): **12 source tracks**, **64 partial slots per track**,
**500 flattened partial lanes**, **16 unison voices**, **16-voice polyphony**.
True unison multiplies oscillator lanes: 64 partials at 16 unison voices =
**1024 oscillator lanes** per voice before FX.

## Realtime Rules

Enforced in `engine/SynthCore` and `engine/Voice`:

- No heap allocation in the audio callback.
- No file I/O in the audio callback.
- No locks, JSON parsing, logging, or sample analysis in `renderBlock()`.
- Control-rate modulation (`engine/ModMatrix`) runs every 32 samples;
  interpolation buffers smooth parameter changes between control-rate blocks.

UI actions may perform non-realtime work (wavetable bake, FFT, file import)
outside the audio `run()`. Any DPF preset or WAV import code must stay on the
UI thread.

## Wavetable Cost

Meta Oscillator tracks use baked multi-frame mip tables. Tables are built in
`dsp/WavetableCore` when `SynthCore::publishSnapshotNoLock()` is called —
never inside the audio callback. Table lookup at runtime is a simple mip-level
interpolation with no per-callback allocation.

## Practical Limits

CPU use is driven by:

| Factor | Notes |
|--------|-------|
| Active voices | 16-voice polyphony pool (`engine/Voice`) |
| Rendered partial lanes | up to 64 per track, 500 flattened across all tracks |
| Unison voices | multiplies partial lanes directly (max 16) |
| Meta Oscillator morph/warp | frame interpolation per voice per partial |
| Source mods (AM/RM/FM/PM/sync) | extra per-voice passes over modulated tracks |
| Per-voice filter chain | up to 4 filter nodes per track, per voice |
| Strip inserts | per-source bus FX applied after voice accumulation |
| Tone FX (`dsp/MasterEffects`) | EQ + filter applied once per block after strip inserts |

Use Panic to clear held voices immediately when testing heavy unison settings.
