# Status

Updated 2026-10-08 at the end of the second session (phase 2). Read this first; details in `docs/sessions/`.

## Where things stand

- Phases 0, 1 and 2 of `docs/ROADMAP.md` are complete: the C++ reference reproduces the JavaScript oracle bit for bit;
  the portable numerics reproduce themselves bit for bit on Windows, Linux and the GPU; the CUDA batch engine runs
  hundreds of simulations per launch and is checked bit for bit against the CPU.
- Builds verified: Windows (Visual Studio 2022 17.14, MSVC 19.44, CMake 4.3, Ninja, CUDA 13.1 on an RTX 4070 Ti
  SUPER) and Linux (WSL Ubuntu 24.04, GCC 13.3). All tests pass on both: 7 on the CPU presets, 9 on the CUDA preset.
- License MIT. The parent folder of this tree is the public JavaScript repository
  `github.com/bochen2029-pixel/titanic-sinking-simulator`, which lists `sinksim/` as an ignored companion; this tree
  is committed locally and has no remote yet.

## Measured

| What | Value |
|---|---|
| Oracle numerics vs oracle golden, Windows and Linux | exact |
| Portable numerics vs portable golden, Windows and Linux | exact |
| GPU trace vs CPU portable numerics | exact |
| Validation table, oracle numerics | 11 of 11 cases match the oracle table |
| Validation table, portable numerics | same verdicts and founder minutes; afloat water within 2 t, one founder time 2.5 s earlier |
| CPU, oracle numerics, one thread | 25,000 steps/s; the 1912 run in 1.5 s |
| CPU, portable numerics (tree emulation), one thread | 12,000 steps/s |
| GPU, double precision, batch of 264 or more | 503,000 steps/s aggregate; 800 full sinkings per minute |

## Next (in order)

1. Mixed precision in the GPU column integrals (single-precision partials, double totals), with the same CPU
   emulation and its own portable reference trace; measure the gain. Double precision is the bottleneck on this part.
2. A batch API over parameter vectors and action schedules (doors, pumps) so calibration and the Carpathia question
   can use the GPU; a Python driver over the CLI or a thin binding.
3. Phase 3 groundwork: the ship-definition schema with source citations and the `shipc` compiler that must reproduce
   `titanic64` exactly before building the deck-level model.

## Open questions

- The oracle's stale-centroid behaviour for nearly empty nodes is kept for exactness; it should become a documented
  scheme v2 change (always compute centroids, or exclude sub-tolerance volumes from the loads).
- Shared memory per block is 13 KB for `titanic64`; a deck-level model with thousands of connections will need the
  per-connection arrays in global memory (a change confined to `engine/cuda/device_step.cuh`).
- GCC 13 reports two `-Wdangling-reference` false positives in the test harness and trace loader; both were
  restructured, to be confirmed on the next Linux build.
