# Determinism: measurements and policy

## What was measured on 2026-10-08

**The shipped golden trace was not reproducible across machines.** Regenerating it with Node 24.16.0 here gave a
file that agreed with the author's for the first 72 one-minute samples and then diverged. The root cause was a single
`Math.pow` result in the hull function differing by one ulp (8.9e-16 in one opening's y coordinate). V8 13.6
dispatches `Math.pow` to the platform C runtime (`v8_flags.use_std_math_pow`), so the author's platform and the
Windows UCRT rounded that one input differently; V8's `sin`, `cos`, `tan`, `asin`, `exp` and `log` are its own
fdlibm port and do not vary by platform.

**The model amplifies ulp-level differences after about an hour of simulated time.** A deliberate one-ulp change of
one parameter (`CdBreach`) stays at the 1e-15 level for 60 minutes and then jumps, between 60 and 70 minutes, when the
first E-deck weir flows start and merge flags flip; by the end of the run the difference is about 0.05° in pitch and
10 to 30 m³ in individual nodes, event times shift by up to 24 s, and the founder time is unchanged. A whole-run
tolerance test therefore measures rounding, not correctness.

| | author's trace vs Node 24 here | one-ulp nudge of CdBreach |
|---|---|---|
| bit-identical until | 72 min | 1 min (1e-15 noise after) |
| visible divergence from | 80 to 90 min | 60 to 70 min |
| largest event shift | 24 s (bulkhead A) | 10 s (bulkhead G) |
| pitch difference at the end | 0.05° | 0.05° |
| founder time | identical | identical |

**The oracle carries hidden state.** Restarting the oracle from a recorded (pose, rates, volumes, levels) reproduced
a continuous run exactly from a fresh object, but not from an object that had already run further: the free-surface
solve updates a node's centroid only when its geometric volume exceeds 1e-9 m³, so a node holding less water than the
solver tolerance keeps an earlier centroid, and the loads still use it. The complete state is therefore pose, rates,
volumes, levels and centroids, and the trace format carries all of them. With that, the oracle reproduces its own
trace from every record with zero difference (`npm run perstep`).

**The C++ reference reproduces the oracle bit for bit.** With strict floating point, the same operation order, the
vendored fdlibm `sin` and `cos` and the V8 `pow` semantics over the platform CRT, `sinksim_check` reports zero
difference at every one of 400 dense single steps, 157 sixty-second windows, and end to end (158 samples, 16 events,
founder at 9440.5 s).

## Policy

1. Physics targets compile with `cmake/StrictFloatingPoint.cmake`: no contraction into fused multiply-add, no
   reassociation, no fast-math. Reductions are in a fixed order; no atomics in the hot path.
2. Transcendental functions are called only through `kernel/math.hpp`. `SINKSIM_MATH=v8` (default) reproduces Node
   24 on Windows; `SINKSIM_MATH=std` is the portable alternative, accepted at tolerance.
3. Acceptance of a build is the three-part check in `sinksim_check`: dense single steps, sampled windows, end to end.
   The CPU reference passes with tolerance 0 (the CTest defaults `SINKSIM_GOLDEN_TOL_STEP` and
   `SINKSIM_GOLDEN_TOL_WINDOW`). Other builds pass at a tolerance recorded in their ADR and are judged end to end by
   the PORTING tolerances: trim within 0.05°, founder within 1 minute, events within 30 s.
4. Golden traces are produced by the oracle on a pinned engine (`.nvmrc`), with the engine and the oracle's core hash
   written into the file, and regenerated only after a recorded physics change.
5. Calibration objectives are built from interpolated curves, never from event crossing times.

## Re-verifying the math on a new machine or compiler

`third_party/fdlibm/build-msvc.cmd` regenerates Node's reference samples and compares fdlibm, the CRT and the `v8math`
header against them (`third_party/README.md`). On a platform where `std::pow` differs from the oracle's CRT, the
dense single-step check will show one-ulp differences in a handful of steps; that is the signal to switch the
comparison to tolerance for that platform and record it.
