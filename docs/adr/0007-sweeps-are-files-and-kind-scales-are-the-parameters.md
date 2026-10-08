# ADR 0007: Sweeps are files, kind scales are the batch parameters, and the GPU's sweep equals the CPU's

Status: accepted, 2026-10-08.

## Context

Calibration, sensitivity and the door-schedule questions all need many runs that differ in a few numbers: the
damage area, the overflow widths, the openings through a deck, which doors are open and when. The compiled
simulation bakes the oracle's parameters into connection areas, so the parameters a driver can vary are connection
fields. Drivers will be written in Python, in notebooks and by other agents, and must not depend on a binding to a
particular build.

## Decision

1. A sweep is a file (`sinksim.sweep`): per instance, scale factors on connection kinds and overrides of
   `enabled`, `tOn` and `area` for selected connections. Results are a file (`sinksim.batch-results`): per
   instance, the verdict, the events and the readout curve at the oracle's history interval. Both are documented
   in `docs/FORMATS.md`.
2. Kind scales are the parameterisation of the Titanic calibration: `breach` is the aggregate damage area, `over`
   the E-deck overflow widths, `down` the openings through E deck, `top` the open share of the top deck; a scale of
   1 on all four is the oracle's calibrated model. (The oracle changes the number of breach openings with the
   area; a scale changes their size. For sweeps around the calibrated point this is the smoother parameterisation.)
3. The same sweep runs on the CPU (`sinksim_sweep`, many threads) and on the GPU (`sinksim_cuda_run --sweep`), and
   the GPU's results equal the CPU's in the same numerics value for value; `sinksim_sweep --compare` enforces it and
   CTest runs it. The curve is recorded on the device when a record is due, as the oracle's run loop does, and the
   final record is computed on the host with the same portable functions, so the bits agree.
4. The objective of the oracle's calibration is exported as data (`sinksim.observations`) and reproduced in
   `tools/py/calibrate_gpu.py`, so a driver scores a run exactly as the oracle did.

## Consequences

- No Python binding is needed for phase 5; a binding can come later for interactive use without changing the
  formats.
- `stopWhenStable` stays a CPU-only option; GPU sweeps run to the founder or to `tMax`.
- A future ship model only needs to name its kinds for the same drivers to work.
