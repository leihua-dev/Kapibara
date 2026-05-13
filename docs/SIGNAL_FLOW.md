# Signal Flow

Realtime audio path:

```text
MIDI / keyboard
-> SynthCore event queue
-> Voice or SamplePlaybackEngine
-> MatrixEngine control update
-> Voice render
-> Tone FX
-> ResamplingEngine / SampleCraft
-> Mix FX
-> output safety buffer
-> audio device
```

Spectral Seed path:

```text
Generator params
-> GeneratorBank
-> SpectralTimeline
-> OperatorChain
-> MatrixEngine
-> Voice render
```

Functional sample path:

```text
sample file
-> offline analysis / track linking
-> FunctionalSpectralSource cache
-> bakeToTimeline
-> SpectralTimeline
```

Ordinary sample path:

```text
sample file
-> SamplePlaybackEngine
-> keyboard pitch control
-> Tone FX / Resampling / Mix FX
```

Realtime-safety notes:

- Functional sample analysis is offline/control-thread work.
- Audio render must not run FFT, file load, JSON parse, or heap-heavy analysis.
- The final output safety buffer is a last-resort anti-pop/anti-clip guard, not a substitute for gain staging.
