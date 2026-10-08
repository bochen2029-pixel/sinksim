# ADR 0003: Determinism policy and how a port is accepted

Status: accepted, 2026-10-08.

## Context

Measured on the original model: a one-ulp difference in one hull coordinate (a `Math.pow` result that differs between
the author's platform and this one) leaves the first 72 minutes of simulated time bit-identical and then, when the
first bulkhead-overtopping flows start, grows to tens of seconds in event times. A one-ulp change of a parameter
behaves the same way. "Match the golden trace to 1e-6 over the whole run" is therefore not a meaningful acceptance
test: it passes or fails on rounding, not on correctness.

## Decision

1. Arithmetic is reproducible by construction: strict floating point on every physics target (no contraction into
   fused multiply-add, no reassociation, no fast-math), fixed-order reductions, no atomics in the hot path.
2. Transcendental functions are a build-time choice (`SINKSIM_MATH`). The default `v8` uses the vendored fdlibm for
   sin and cos and V8 13.6's `pow` semantics (special cases, then the platform CRT), which reproduces Node 24 bit for
   bit on Windows (`third_party/README.md`). `std` uses the platform `<cmath>` for portability.
3. The golden trace carries the complete state at every record (pose, rates, volumes, levels, centroids). A port is
   accepted on three tests, in this order: every dense record stepped once matches the next record; every sampled
   record stepped to the next matches it; the run from the initial state matches the samples, events and founder time.
   The CPU reference with `v8` math passes all three exactly (tolerance 0). A build with different math, or the GPU, is
   accepted at tolerance on the first two and judged on the third by the PORTING tolerances (trim within 0.05°,
   founder within 1 minute, events within 30 s).
4. The oracle is regenerated only on a pinned engine, with the engine version written into the trace, and only after
   a recorded physics change.

## Consequences

- Calibration objectives must be smooth functions of interpolated curves, not of event crossings, because event times
  jitter at the ulp level.
- Any physics change invalidates the golden trace and the calibration together; the procedure in `AGENTS.md` applies.
- The oracle's hidden state (centroids of nearly empty nodes) is documented in `docs/PHYSICS.md` and kept; fixing it
  is a physics change for a later scheme version.
