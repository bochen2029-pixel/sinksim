# Architecture

## Layers

```
 sources, plans, testimony
          │
          ▼
  ship definition ──► ship compiler ──► compiled ship (.ship.json)     geometry: columns, segments, nodes
  scenario            (tools/js now,    compiled sim  (.sim.json)      network, body, events, initial state
  parameters           tools/py/shipc
  actions              in phase 3)             │
                                               ▼
                                          engine (C++)
                              kernel: pose → buoyancy → levels → flows → volumes → body → events
                              host: model, loaders with hash checks, Simulation, traces
                                               │
                     ┌─────────────────────────┼──────────────────────────┐
                     ▼                         ▼                          ▼
              CPU reference              CUDA batch (phase 2)       WebAssembly (phase 6)
              apps, tests                calibration, rollouts      viewer
                     │
                     ▼
            traces, validation tables, reports (data/, docs/)
```

The oracle (`oracle/js`) sits beside this as the frozen source of truth for scheme v1; the JS tools drive it to
produce the compiled files and the golden trace that the engine is tested against.

## The kernel

`engine/include/sinksim/kernel/`, one header per stage, all functions `SS_HD` (host and device), operating on views:

| Header | Stage |
|---|---|
| `types.hpp` | views over the compiled arrays, the per-step records, `State`, `Scratch`, `Outputs`, `EventLog` |
| `scheme.hpp` | the numerical constants of scheme v1 |
| `math.hpp` | the math policies `OracleMath`, `PortableMath`, `StdMath`; the only place transcendental functions are called |
| `fdlibm_portable.hpp` | fdlibm's sin, cos, pow and scalbn as host-and-device functions |
| `reduce.hpp` | partials and the canonical reduction tree shared by the GPU and its CPU emulation |
| `frame.hpp` | rotation and translation of the pose |
| `hydro.hpp` | the column integrals in serial order and in tree order; the pass functors |
| `levels.hpp` | free-surface solves for single nodes and shared-surface groups, driven by a pass functor; one entry per group |
| `flows.hpp` | per-node and per-connection pieces of the flow phase, the incidence gather, the volume update; the serial driver |
| `body.hpp` | loads and the rigid-body integration |
| `events.hpp` | monitors, marks, the founder rule |
| `step.hpp` | the step, in the oracle's order, templated on the math policy and the reduction order |
| `readouts.hpp` | trim, list, drafts, tonnes |

Rules: no allocation, no exceptions, no virtual calls, no standard containers; everything the step needs arrives
through `ShipView`, `SimView`, `State`, `Scratch`. The pass functor seam is where the GPU substitutes a
warp-cooperative column reduction.

## The GPU

`engine/cuda/device_step.cuh` runs the same functions in one thread block per simulation: thread 0 sets the pose;
every thread sums its share of columns and the warps combine in the canonical order; each warp solves whole groups
with a warp-cooperative node pass; threads take nodes and connections for the flow phase; thread 0 runs the serial
tail (excess, loads, integration, events) exactly as the CPU does. `engine/cuda/batch.cu` owns the device memory,
launches in bounded chunks and assembles results and traces; `sinksim/cuda/batch.hpp` is the CUDA-free interface.

## The host side

`model.hpp` owns the arrays (`CompiledShip`, `CompiledSim`), defines `StateSnapshot` and derives the incidence
lists. `io/compiled.*` loads the files and verifies every hash; `io/trace.*` reads and writes traces; `io/json.*`
wraps nlohmann with the project's layout and number formatting. `simulation.*` binds views, drives steps with the
chosen `Numerics`, records traces, applies actions and offers diagnostics. `sweep.*` reads sweep files, applies an
instance's scales and overrides to any target through three setters, runs sweeps on the CPU and writes and compares
results. Apps are thin wrappers: run, check, validate, sweep, and the CUDA batch runner (which also runs sweeps).
`tools/py/sweep.py` and `calibrate_gpu.py` drive the tools from Python through the files alone.

## Data flow of a check

1. `tools/js/export_compiled.js` builds the oracle's ship and each scenario, writes the compiled files with hashes.
2. `tools/js/golden.js` runs the oracle and writes the trace with the complete state at every record;
   `tools/js/perstep.js` proves the oracle reproduces itself from every record.
3. `sinksim_check` loads the compiled files (refusing hash mismatches), restarts from every record, steps, compares;
   then runs end to end. `ctest` runs it with tolerance zero.
4. `sinksim_validate` runs the catalog with the oracle's options and compares the table with
   `data/validation/titanic64/validation.oracle.json`.

## Extension points

- **A new ship**: a compiler writes `ships/<name>/<model>.ship.json` and `sims/`; nothing in the engine changes. The
  engine does not know what a bulkhead or a deck is; those are groups, connections, monitors and marks.
- **A new scheme variant** (implicit flows, capacity tables): a new header beside the existing one, selected at the
  step level, with its own scheme version, golden trace and calibration.
- **Mixed precision on the GPU**: single-precision partials in `reduce.hpp` with the CPU emulating them, a new
  portable reference trace, no change to the solvers.
- **WebAssembly**: the same engine library compiled with Emscripten, exposing load, step, restore and snapshot.
- **Actions**: only connection fields change (`en`, `area`, `tOn`), so rollouts never reallocate.

## What is deliberately absent

No scenario logic in C++ yet (scenarios are compiled by the JS tools from the oracle), no viewer in this tree, no
physics beyond scheme v1. Each arrives in its roadmap phase with its own acceptance test.
