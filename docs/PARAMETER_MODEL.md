# Parameter Model

Current parameter state is still split across structs. This document defines the target direction so future code does not spread parameter ownership further.

Current owners:

- `SourceGenParams`: generator and seed-shaping parameters.
- `OperatorChain`: non-destructive spectral operator slots.
- `GlobalAdsrParams`: shared performance envelope.
- `MatrixRule`, `LfoParams`, `ChaosParams`, `ShapeSourceParams`: modulation system.
- `EffectsChainParams`: Tone FX and Mix FX.
- `ResamplingEngineParams`: SampleCraft buffer processing.
- `CompositionProject`: Seed/Creator/Motif/Structure/Mixer project state.

Target rule:

- Public parameters should eventually register through a `ParameterManager`.
- UI controls should bind to parameter IDs rather than hand-copying fields forever.
- Presets should serialize by stable parameter IDs and schema version.
- Note-level PianoLocks should store overrides, not copied full preset state.

Matrix source semantics:

- Time sources: LFO1..LFO8, ADSR, Chaos.
- Event/performance sources: Velocity, KeyTrack, Random.
- Static partial-axis source: Shape.
- GeneratorSelf maps current partial frequency position into `[-1, 1]`.

Priority model, low to high:

```text
Base preset
< Seed / Creator automation
< Motif automation
< Structure automation
< Mixing automation
< Note parameter locks
< Live temporary performance override
```
