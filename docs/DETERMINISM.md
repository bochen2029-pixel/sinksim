# Determinism: measurements and policy

## Three sets of numerics

| `Numerics` | sin, cos | pow | summation order | reproduces |
|---|---|---|---|---|
| `oracle` | vendored fdlibm (what V8 uses) | the platform CRT after V8's special cases (`third_party/v8math`) | the oracle's serial loops | the JavaScript model bit for bit on Windows; on Linux too for the 1912 run (glibc and the UCRT agreed on every pow the run called) |
| `portable` | host-and-device fdlibm port | host-and-device fdlibm port | the GPU's canonical trees (`kernel/reduce.hpp`) | itself, bit for bit, on every CPU and on the GPU |
| `std` | platform `<cmath>` | platform `<cmath>` | serial | nothing in particular; a portability fallback |

The portable and oracle numerics differ by at most one ulp in `pow` and by the summation order; measured on the
1912 run, that is a difference of 2e-15 in pose and 4e-14 m³ in volumes after one step, and the usual amplification
after the first overtopping (below).

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

| | author's trace vs Node 24 here | one-ulp nudge of CdBreach | portable vs oracle numerics |
|---|---|---|---|
| bit-identical until | 72 min | 1 min (1e-15 noise after) | never (1e-15 noise from the first step) |
| visible divergence from | 80 to 90 min | 60 to 70 min | 70 min |
| largest event shift | 24 s (bulkhead A) | 10 s (bulkhead G) | 11.5 s (bulkhead G) |
| pitch difference at the end | 0.05° | 0.05° | 0.3° (last sample, in the plunge) |
| founder time | identical | identical | 0.5 s earlier |

**The oracle carries hidden state.** Restarting the oracle from a recorded (pose, rates, volumes, levels) reproduced
a continuous run exactly from a fresh object, but not from an object that had already run further: the free-surface
solve updates a node's centroid only when its geometric volume exceeds 1e-9 m³, so a node holding less water than the
solver tolerance keeps an earlier centroid, and the loads still use it. The complete state is therefore pose, rates,
volumes, levels and centroids, and the trace format carries all of them. With that, the oracle reproduces its own
trace from every record with zero difference (`npm run perstep`).

**The C++ reference reproduces the oracle bit for bit.** With strict floating point, the same operation order, the
vendored fdlibm `sin` and `cos` and the V8 `pow` semantics over the platform CRT, `sinksim_check` reports zero
difference at every one of 400 dense single steps, 157 sixty-second windows, and end to end (158 samples, 16 events,
founder at 9440.5 s). The same holds on Linux with GCC.

**The GPU reproduces the CPU's portable numerics bit for bit**, and the Linux CPU reproduces the Windows CPU's
portable trace bit for bit: zero difference in all three checks. The portable reference is
`data/golden/titanic64/titanic.portable.trace.json`.

**The validation table under the portable numerics** has the same verdicts and the same founder times to the minute
as under the oracle numerics. Two afloat equilibria differ by 1 to 2 t of water because the stop-when-stable rule
ends those runs a few seconds apart while a trickle is still entering, and the run without the boiler-room-5 seam
founders 2.5 s earlier; that is the measured size of the numerics difference after three hours of simulated time.

## Policy

1. Physics targets compile with `cmake/StrictFloatingPoint.cmake`: no contraction into fused multiply-add, no
   reassociation, no fast-math, on the host and on the device (`-fmad=false`). Reductions are in a fixed order; no
   atomics anywhere.
2. Transcendental functions are called only through the math policies in `kernel/math.hpp`.
3. Acceptance of a build is the three-part check in `sinksim_check`: dense single steps, sampled windows, end to end,
   with tolerance zero: the oracle numerics against the oracle trace, the portable numerics against the portable
   trace, and the GPU's trace against the CPU's portable numerics. The PORTING tolerances (trim within 0.05°, founder
   within 1 minute, events within 30 s) remain the bar for comparing different numerics with each other.
4. Golden traces are produced on a pinned engine (`.nvmrc` for the oracle; the engine version for the portable
   trace), with the producer written into the file, and regenerated only after a recorded physics or numerics change.
5. Calibration objectives are built from interpolated curves, never from event crossing times.

## The C runtime

`Math.pow` in the oracle is the platform CRT's `pow`. The parallel session that verified `v8math` against 260,009
Node samples found the agreement to hold with the static MSVC runtime, so the engine links the static runtime
(`CMAKE_MSVC_RUNTIME_LIBRARY` in the root `CMakeLists.txt`). The 1912 run uses `pow` only with the exponents 1.5 and
0.385 over a narrow range of ratios and was exact with either runtime; the static one removes the dependency on
whichever UCRT DLL a machine happens to have. On the 200,000 weir-law inputs `tests/test_portable_math.cpp` samples,
the platform `pow` and fdlibm's agree on all but a small fraction; the test prints the fraction for the machine it
runs on.

## Re-verifying the math on a new machine or compiler

`third_party/fdlibm/build-msvc.cmd` regenerates Node's reference samples and compares fdlibm, the CRT and the `v8math`
header against them (`third_party/README.md`). `test_portable_math` holds the host-and-device port against the C
build. If the oracle check on a new platform shows one-ulp differences at a few dense steps, the platform's `pow`
differs from the oracle's CRT for some inputs; record it and compare that platform at tolerance, or use the portable
numerics, which do not depend on the platform.
