# Performance Notes

The main DSP cost is additive oscillator lane count:

```text
rendered partial slots  ×  unison voices  ×  active voices
```

The Source Rack UI allows free track creation, but the audio renderer clamps
the flattened realtime budget to **64 partial lanes** and **16 source tracks**.
True unison multiplies oscillator lanes: 64 partials at 16 unison voices =
**1024 oscillator lanes** before tone FX.

## Realtime Rules

Enforced in `engine/SynthCore` and `engine/Voice`:

- No heap allocation in the audio callback.
- No file I/O in the audio callback.
- No locks, JSON parsing, logging, or sample analysis in `renderBlock()`.
- Control-rate modulation (`engine/MatrixEngine`) runs every 32 samples;
  interpolation buffers smooth parameter changes between control-rate blocks.

UI actions may perform non-realtime work (wavetable bake, FFT, file import)
outside the audio `run()`. Any DPF preset or WAV import code must stay on the
UI thread.

## Wavetable Cost

Meta Oscillator tracks use baked multi-frame mip tables. Tables are built in
`dsp/Generators` when `SynthCore::publishSnapshotNoLock()` is called — never
inside the audio callback. Table lookup at runtime is a simple mip-level
interpolation with no per-callback allocation.

## Practical Limits

CPU use is driven by:

| Factor | Notes |
|--------|-------|
| Active voices | 16-voice polyphony pool (`engine/Voice`) |
| Rendered partial lanes | up to 64 across all tracks |
| Unison voices | multiplies partial lanes directly |
| Meta Oscillator morph/warp | frame interpolation per voice per partial |
| Tone FX (`dsp/Effects`) | EQ + filter applied once per block after voice sum |

Use Panic to clear held voices immediately when testing heavy unison settings.
