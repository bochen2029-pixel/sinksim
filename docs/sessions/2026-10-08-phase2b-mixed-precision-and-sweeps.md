# Session 2026-10-08 (third): mixed precision and the sweep interface

Harness and model: Claude Code (Claude Fable 5.1). A parallel session published the repository as
`github.com/bochen2029-pixel/sinksim` during this work; the branch was in sync with the remote when this session's
commits were made on top.

## Starting point

Phase 2 done in double precision; `docs/STATUS.md` listed mixed precision, a batch parameter API and the
ship-definition schema as the next three things.

## What was done

1. Mixed precision: `FrameF`, single-precision partials and column functions in `kernel/reduce.hpp`, the mixed
   tree integrals in `kernel/hydro.hpp`, `Order::TreeMixed` in `kernel/step.hpp`, `Numerics::PortableMixed`
   ("portable32"); the reference trace `data/golden/titanic64/titanic.portable32.trace.json` and its CTest.
2. The GPU in mixed precision: `WarpPasserMixed`, the single-precision block hydro, `run_chunk<Mixed>`,
   `--precision mixed|double`, two parity fixtures in CTest.
3. Sweeps: `sinksim/sweep.hpp` (formats, apply through setters, threaded CPU runner, exact comparison),
   `sinksim_sweep`, device-side readout curves in the CUDA batch (`curveEvery`, `curve()`, `final_readouts()`),
   `sinksim_cuda_run --sweep --results`, a smoke sweep and the CPU-versus-GPU sweep test.
4. `tools/js/export_observations.js` (Halpern's targets and the objective's weights as data), `tools/py/sweep.py`,
   `tools/py/calibrate_gpu.py` (batched search, `--evaluate`).
5. ADR 0007, FORMATS, DETERMINISM, STATUS, ROADMAP, README, ARCHITECTURE, AGENTS, CHANGELOG.

## What was measured

- GPU mixed vs CPU portable32: exact (400 steps, 157 windows, end to end). GPU sweep vs CPU sweep: identical.
- Mixed precision vs double: 5e-8 in pose after one step, founder 1 s later, events within 23 s.
- Throughput, mixed precision: 92,000 steps/s at 8 instances; 1,194,000 at 264; 1,167,000 at 1,056
  (1,900 full sinkings per minute). CPU sweep at 16 threads: 48,000 steps/s.
- Calibration demo: 5 rounds of 264 on the GPU, 1,320 evaluations in 54 s; J 10.408 at the oracle's parameters
  (10.411 in oracle numerics), best found 10.326 at scales breach 0.998, over 0.891, down 0.924, top 1.097.

## Open questions and loose ends

- The 2.4 gain from mixed precision is well below the arithmetic ratio; the serial tail on thread 0 and the twelve
  synchronisations per step are the suspects. Profile before changing anything.
- The breach kind scale changes opening size, the oracle changed opening count (ADR 0007).

## Next

1. Nsight profile of the GPU step and the optimisation it points to, with new portable references if bits change.
2. Ship-definition schema and `shipc` (phase 3); the list needs topology, not parameters.
3. Phase 5 on the sweep interface: priors, sampler, bands; the Carpathia door schedules.
