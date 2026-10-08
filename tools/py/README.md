# Python tools

Analysis helpers that need nothing beyond the standard library (numpy where noted). They read the engine's data files
and never run the physics.

| Script | What it does |
|---|---|
| `trace_diff.py A B` | compares two `sinksim.trace` files: first differing record, per-field maxima, the divergence profile over time, event shifts. Use it to compare an engine trace with the golden trace, or two engine builds (CPU against CUDA, one compiler against another). |

Reserved for later phases: the ship compiler (`shipc`, phase 3) that turns a ship definition with deck plans into the
compiled format, the calibration and uncertainty drivers (phase 5), and plotting of validation reports.
