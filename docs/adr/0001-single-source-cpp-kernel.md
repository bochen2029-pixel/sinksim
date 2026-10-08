# ADR 0001: One C++ kernel source for CPU, CUDA and WebAssembly

Status: accepted, 2026-10-08.

## Context

The project started as a JavaScript model with a three.js viewer. The handoff planned a C++ port for speed, a CUDA
batch engine for calibration and rollouts, and possibly a WebAssembly build so the viewer would run the same code.
Three implementations of one physics would drift; two had already started to (the viewer's live core and the headless
core were the same file only by discipline).

## Decision

The physics lives once, in `engine/include/sinksim/kernel/`, as plain functions over raw-pointer views with the
`SS_HD` annotation, no allocation and no standard-library containers in the hot path. The CPU reference, the CUDA
build and the WebAssembly build compile those same headers. Host-side ownership, IO and drivers are separate. The
JavaScript model becomes the frozen oracle and is retired from production use once the WebAssembly build reaches
parity.

The level solvers take the column pass as a callable so that the GPU can substitute a block-cooperative reduction
without touching the solver logic.

## Consequences

- No JAX or Python rewrite: the browser target would need a second implementation, and gradients are not needed when
  batches of thousands are cheap.
- Kernel code must stay device-agnostic: no exceptions, no virtual calls, no dynamic memory, math through
  `kernel/math.hpp` only.
- Precision templating is deferred: the GPU's mixed precision will live in its own pass functors, not in the scalar
  solver and body code, which stay double.
