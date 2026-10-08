# Python tools

Drivers and analysis helpers that need nothing beyond the standard library. They never run the physics; they talk
to the engine's command-line tools through files (`docs/FORMATS.md`), so they work with any build on any machine.

| Script | What it does |
|---|---|
| `sweep.py` | Build a sweep (instances with kind scales and connection overrides), run it with `sinksim_sweep` (CPU) or `sinksim_cuda_run --sweep` (GPU), read the results; `interpolate` reads a curve the way the oracle's calibration does. |
| `calibrate_gpu.py` | The oracle's calibration objective over the four flow kinds, evaluated in batches of hundreds on the GPU; `--evaluate results.json` scores any results file. A demonstration of the batch path; phase 5 adds priors and bands. |
| `trace_diff.py A B` | Compares two `sinksim.trace` files: first differing record, per-field maxima, the divergence profile over time, event shifts. |

Reserved for later phases: the ship compiler (`shipc`, phase 3) that turns a ship definition with deck plans into the
compiled format, the inference drivers (phase 5) and plotting of validation reports.
