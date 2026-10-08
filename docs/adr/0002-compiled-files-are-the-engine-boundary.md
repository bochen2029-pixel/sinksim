# ADR 0002: Compiled ship and simulation files are the engine boundary

Status: accepted, 2026-10-08.

## Context

The original model built its geometry (parametric hull, columns, nodes) and its network (bulkheads, doors, overflow
paths, breaches) in code, specific to Titanic. A general engine must not know what a bulkhead is, and a deck-level
Titanic model with hundreds of spaces is a data change, not a kernel change.

## Decision

The engine consumes compiled files only (`docs/FORMATS.md`): columns, segments, nodes, free-surface groups,
connections, monitors, marks, rigid-body constants and the initial state, all as arrays with no hard-coded counts.
Anything that produces those files is a ship compiler and lives in the tools layer: today `tools/js/export_compiled.js`
drives the frozen oracle; roadmap phase 3 adds `tools/py/shipc`, which must reproduce `titanic64` exactly before it
builds anything new.

Actions (doors, pumps, counter-flooding) only change connection fields (`en`, `area`, `tOn`); they never change array
shapes, so one compiled simulation serves a whole batch of rollouts.

## Consequences

- The oracle's compartment-specific logic (`nodeIndex`, pair merging, bulkhead overflow events) is expressed as data:
  groups with modes, connections with `skipGroup` and `monitor` references.
- Every numeric array is hashed by the compiler and verified by the engine, so a file that did not round-trip exactly
  is refused rather than simulated.
- The viewer's interactive hole tool becomes a spatial query over the compiled data (nearest column, containing
  segment), to be provided by the engine, not by hull formulas in the viewer.
