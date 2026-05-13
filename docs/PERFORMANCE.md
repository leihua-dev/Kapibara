# Performance Notes

The main realtime cost is:

```text
partialCount * unisonVoices * activeVoices
```

Example:

```text
182 partials * 16 unison = 2912 oscillator lanes per note
```

With 4 held notes this becomes 11648 oscillator lanes before Matrix, FX, resampling, UI repaint, or audio driver overhead. That is a large CPU load for a realtime JUCE app.

Current bottlenecks:

- `Voice::renderAdd`: nested loops over samples, active partials, and unison voices.
- Per-partial ADSR state and per-block smoothing are updated for every active partial.
- Matrix is evaluated per voice and partial at control rate.
- High unison multiplies oscillator phase accumulation and pan/gain mixing.

Near-term optimization plan:

1. Add quality/performance modes that cap effective partials and unison separately from preset values.
2. Skip inaudible partials earlier using amplitude and Nyquist thresholds.
3. Add voice-level CPU budget fallback: reduce unison or partials when polyphony rises.
4. Precompute per-block constants for unison and phase increments.
5. Consider SIMD or wavetable oscillator paths after the architecture is stable.

Practical working limits for now:

- Sound design preview: 64-96 partials, 1-4 unison.
- Dense spectral inspection: 128-182 partials, 1-2 unison.
- 16 unison should be treated as a special effect, not the default for high partial counts.
