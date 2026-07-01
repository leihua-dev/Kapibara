# Wavetable presets

Place `.wav` wavetable files or Kapibara `.kwt` harmonic tables here.
Subdirectories are supported and appear as categories in the wavetable menu.

Meta Oscillator loads the full table. Partial Bank can load the same `.kwt`
files and maps the first 64 harmonics of each frame to its 64 partial amp/phase
lanes.

New `.kwt` saves use the compact binary `KWT2` format:
`KwtHeader { magic='KWT2', frameCount, binCount }` followed by packed
`uint16 amplitude + int16 phase` bins. Older ASCII `KAPIBARA_WT` files remain
loadable.
