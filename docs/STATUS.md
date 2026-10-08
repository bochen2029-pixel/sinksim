# Status

Updated 2026-10-08 at the end of the third session (mixed precision and sweeps). Read this first; details in
`docs/sessions/`.

## Where things stand

- Phases 0, 1 and 2 of `docs/ROADMAP.md` are complete, including the GPU's production numerics and the batch
  interface: the C++ reference reproduces the JavaScript oracle bit for bit; the portable numerics (double and mixed
  precision) reproduce themselves bit for bit on Windows, Linux and the GPU; sweeps run on the CPU and the GPU with
  identical results; a Python driver calibrates on the GPU with the oracle's own objective.
- Builds verified: Windows (Visual Studio 2022 17.14, MSVC 19.44, CMake 4.3, Ninja, CUDA 13.1 on an RTX 4070 Ti
  SUPER) and Linux (WSL Ubuntu 24.04, GCC 13.3). Tests: 9 on the CPU presets, 15 on the CUDA preset, all passing.
- License MIT. Published as `github.com/bochen2029-pixel/sinksim`. The parent folder of this tree is the public
  JavaScript repository `github.com/bochen2029-pixel/titanic-sinking-simulator`, which lists `sinksim/` as an
  ignored companion; that repository's model is byte-identical to `oracle/js`.

## Measured

| What | Value |
|---|---|
| Oracle numerics vs oracle golden, Windows and Linux | exact |
| Portable and portable32 numerics vs their goldens | exact; the GPU matches both exactly |
| GPU sweep vs CPU sweep (curves, events, finals) | identical value for value |
| Validation table, oracle numerics | 11 of 11 cases match the oracle table |
| CPU, oracle numerics, one thread | 25,000 steps/s; the 1912 run in 1.5 s |
| CPU sweep, 16 threads, portable32 | 48,000 steps/s aggregate |
| GPU, double precision, batch of 264 or more | 503,000 steps/s; 800 full sinkings per minute |
| GPU, mixed precision, batch of 264 or more | 1,190,000 steps/s; 1,900 full sinkings per minute |
| Calibration demo, 5 rounds of 264 on the GPU | 1,320 evaluations in 54 s (the oracle: 89 in 4 min) |
| Objective at the oracle's parameters | J = 10.41 (both numerics); best found 10.33, the list terms dominate |

## Next (in order)

1. Profile the GPU step (Nsight) and attack what the profile shows; the serial tail on thread 0 and the phase
   synchronisation are the suspects. Any change to the bits gets new portable references.
2. Phase 3 groundwork: the ship-definition schema with source citations and the `shipc` compiler that must
   reproduce `titanic64` exactly before building the deck-level model. The list cannot be fixed by calibration
   (measured: the list terms dominate the objective and no flow parameter moves them); it needs the deck model.
3. Phase 5 proper on top of the sweep interface: priors, an ensemble sampler, posterior bands; then the door
   schedules of the Carpathia question.

## Open questions

- The oracle's stale-centroid behaviour for nearly empty nodes is kept for exactness; it should become a documented
  scheme v2 change (always compute centroids, or exclude sub-tolerance volumes from the loads).
- Shared memory per block is 13 KB for `titanic64`; a deck-level model with thousands of connections will need the
  per-connection arrays in global memory (a change confined to `engine/cuda/device_step.cuh`).
- The sweep parameterisation scales the breach openings; the oracle changed their number with the area. Equivalent
  around the calibrated point, documented in ADR 0007.
