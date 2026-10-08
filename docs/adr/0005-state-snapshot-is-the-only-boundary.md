# ADR 0005: The state snapshot is the only runtime boundary

Status: accepted, 2026-10-08.

## Context

The viewer, the trace format, the golden tests, the future rollout engine and the future WebAssembly binding all need
to read or restore "where the simulation is". The original model exposed a live object; the viewer read its internals
directly.

## Decision

The complete dynamic state is the snapshot: time, pose, rates, and per node the volume, level and centroid
(`StateSnapshot`). Everything else the step uses is either static data or recomputed within the step. The snapshot is
what traces store, what `Simulation::restore` accepts, what rollouts branch from and what the viewer will render.
Derived per-step quantities (areas, merged flags, flows, buoyancy) are outputs, exposed read-only.

## Consequences

- Restarting from a snapshot and stepping must reproduce a continuous run exactly; the golden test enforces it.
- Adding state (for example trapped-air pressure in a later scheme) means extending the snapshot and the trace format
  with a version bump, never a side channel.
- The viewer never computes physics; it asks the engine for snapshots and derived quantities.
