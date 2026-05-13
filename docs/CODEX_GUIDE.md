# Codex Guide

Read these files first for broad tasks:

1. `AGENTS.md`
2. `docs/ARCHITECTURE.md`
3. `docs/SIGNAL_FLOW.md`
4. `docs/PARAMETER_MODEL.md`
5. `MotifForge中心架构.md` for the long-form design record

Common commands:

```powershell
cmake --build build --config Release
```

For smoke testing:

```powershell
$exe = Join-Path (Resolve-Path .) 'build\JuceAdditiveSynth_artefacts\Release\MotifForge v0.3.exe'
$p = Start-Process -FilePath $exe -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 3
if(-not $p.HasExited){ Stop-Process -Id $p.Id -Force }
```

Coding rules:

- Use `rg` for search.
- Keep DSP changes small and separately verifiable.
- Do not allocate, lock, parse JSON, load files, or run analysis in the audio callback.
- Prefer moving new UI code into `src/ui/`.
- Update docs when changing architecture, signal flow, or preset schema.
- Run a Release build before finalizing behavior changes.

Known hotspots:

- `src/Main.cpp` is still too large. Extract panels gradually.
- `Voice.cpp` is the main CPU hotspot for high partial and unison counts.
- `FunctionalSampleSource.cpp` is intentionally analysis-heavy and should stay off the audio thread.
