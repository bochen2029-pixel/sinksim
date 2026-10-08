# ADR 0006: Portable numerics, and the GPU is checked bit for bit against them

Status: accepted, 2026-10-08.

## Context

The CUDA batch engine was the next phase. A GPU cannot call the host libm or the C runtime, its summation order
differs from a serial loop, and the model amplifies rounding differences into event-time differences of tens of
seconds after an hour of simulated time (ADR 0003). Checking the GPU "at tolerance" would therefore pass bugs that
only move results by rounding and fail correct code that crossed a threshold one step earlier.

## Decision

1. A second set of numerics, **portable**, exists beside the oracle's: fdlibm's `sin`, `cos`, `pow` and `scalbn`
   transliterated as host-and-device functions (`kernel/fdlibm_portable.hpp`, held bit for bit against the
   vendored C build by `tests/test_portable_math.cpp`), and a canonical reduction tree (`kernel/reduce.hpp`): 32
   lane partials and a halving tree for a node pass, 256 thread partials, warp trees and an in-order combine for
   the buoyancy integral. The CPU emulates that order exactly; the GPU performs it with shuffles.
2. Every other phase of the step is written so that its summation order does not depend on the processor:
   per-node accumulation of flows is a gather over incident connections in increasing connection order, which is
   the order a serial scatter used; the sea inflow, the excess return, the loads and the integration run as one
   serial thread on the GPU, exactly as on the CPU.
3. The GPU always computes the portable numerics. Its acceptance test is `sinksim_check --numerics portable`
   against a trace the GPU wrote: tolerance zero, step by step, window by window and end to end.
4. Numerics are a per-simulation runtime choice (`Numerics::Oracle`, `Portable`, `Std`), not a build option, so
   one binary holds the oracle-exact reference and the GPU-exact emulation.
5. Kernel launches are chunked (2,000 steps by default) so that a display GPU's watchdog never interrupts a batch;
   state lives in global memory between launches and in shared memory within one.
6. Per instance, only the connection fields `en`, `area` and `tOn` and the state vary; geometry and the immutable
   network are shared by every block.

## Measured (RTX 4070 Ti SUPER, double precision, 256 threads per block)

| batch | aggregate steps/s | full 1912 sinkings per minute |
|---|---|---|
| 8 | 51,000 | 81 |
| 66 | 411,000 | 654 |
| 264 | 503,000 | 800 |
| 1,056 | 503,000 | 800 |

The GPU trace matches the CPU's portable numerics exactly, and the Linux build matches the Windows portable trace
exactly. Double precision runs at one sixty-fourth of single rate on this consumer part, which is why the
saturated rate is only about 1.3 times a fully threaded 16-core CPU reference.

## Consequences

- The next optimisation is mixed precision in the column integrals (single-precision partials, double totals),
  implemented with the same CPU emulation so that it stays bit-checkable; it changes the bits and therefore gets its
  own portable reference trace.
- The oracle numerics stay the scientific reference for calibration against history; the portable numerics are the
  reference for batch work, with a measured, documented difference between the two (docs/DETERMINISM.md).
- Larger ship models need more shared memory per block; above the device limit the per-connection arrays move to
  global memory, a change confined to the device step.
