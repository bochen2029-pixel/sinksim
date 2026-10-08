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
| `math.hpp` | the only place transcendental functions are called; implementation chosen at build time |
| `frame.hpp` | rotation and translation of the pose |
| `hydro.hpp` | the column integrals: buoyancy of the envelope, water in a node below a level; the serial pass functor |
| `levels.hpp` | free-surface solves for single nodes and shared-surface groups, driven by a pass functor |
| `flows.hpp` | effective heads and areas, degrees, orifice and weir flows with the limiter, volume update |
| `body.hpp` | loads and the rigid-body integration |
| `events.hpp` | monitors, marks, the founder rule |
| `step.hpp` | the step, in the oracle's order |
| `readouts.hpp` | trim, list, drafts, tonnes |

Rules: no allocation, no exceptions, no virtual calls, no standard containers; everything the step needs arrives
through `ShipView`, `SimView`, `State`, `Scratch`. The pass functor seam is where the GPU substitutes a
block-cooperative column reduction.

## The host side

`model.hpp` owns the arrays (`CompiledShip`, `CompiledSim`) and defines `StateSnapshot`. `io/compiled.*` loads the
files and verifies every hash; `io/trace.*` reads and writes traces; `io/json.*` wraps nlohmann with the project's
layout and number formatting. `simulation.*` binds views, drives steps, records traces, applies actions and offers
diagnostics. Apps are thin wrappers: run, check, validate.

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
- **The GPU**: block-per-simulation driver, gather-ordered accumulation, pass functors over shared memory; same
  solver and body code.
- **WebAssembly**: the same engine library compiled with Emscripten, exposing load, step, restore and snapshot.
- **Actions**: only connection fields change (`en`, `area`, `tOn`), so rollouts never reallocate.

## What is deliberately absent

No scenario logic in C++ yet (scenarios are compiled by the JS tools from the oracle), no viewer in this tree, no
physics beyond scheme v1. Each arrives in its roadmap phase with its own acceptance test.
