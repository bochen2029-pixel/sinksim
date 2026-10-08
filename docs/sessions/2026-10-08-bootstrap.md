# Session 2026-10-08: bootstrap

Harness and model: Claude Code (Claude Fable 5.1), with a parallel session of the same project fetching and
verifying the third-party math (see `third_party/README.md`).

## Starting point

A JavaScript flooding model handed over as a zip (`oracle/js`), with a HANDOFF asking for a C++ port, then CUDA, then
physics. No repository, no tests beyond the model's own scripts, a golden trace that had been produced on another
machine.

## What was done

1. Measured the handoff's golden trace against a Node 24 regeneration and found the one-ulp `Math.pow` difference and
   the model's amplification after about an hour (`docs/DETERMINISM.md`).
2. Created the repository: imported the oracle verbatim (tag `oracle-js-verbatim`), froze it with a manifest, made
   two tool-level edits (`oracle/README.md`).
3. Node tools: scenario catalog, compiled-file exporter with FNV hashes, golden writer, self-check, validation runner.
   The self-check found the oracle's hidden state (centroids of nearly empty nodes); the trace and compiled formats
   now carry centroids and the self-check is exact.
4. The C++ engine: single-source kernel over views (pose, hydro, levels with a pass functor, flows, body, events,
   step, readouts), host model, JSON layer on vendored nlohmann, loaders with hash verification, trace IO, the
   `Simulation` driver, three apps, a dependency-free test harness, CMake with a strict-floating-point interface
   target and presets, the MSVC wrapper.
5. Vendored fdlibm (prefixed build), the `v8math` pow wrapper and the V8 reference files; routed the kernel's sin,
   cos and pow through `kernel/math.hpp` with `SINKSIM_MATH=v8` as the default.
6. Documentation: AGENTS and CLAUDE, architecture, physics, formats, determinism, validation, roadmap, sources, five
   ADRs, this log.

## What was measured

- Oracle self-check: 400 single steps and 157 windows, worst difference 0.
- Engine against the golden trace: dense steps 0, windows 0, end to end 0; 16 events identical; founder 9440.5 s.
- Validation: 11 of 11 cases match the oracle table.
- Throughput: engine 25,100 steps/s, oracle 12,000 steps/s (single thread, this machine).
- Build: MSVC 19.44 `/fp:strict`, no warnings in project code (the vendored fdlibm emits one `#ident` warning).

## Open questions and loose ends

- The first build wrapper used an environment variable named `RC`, which CMake reads as the resource compiler; fixed,
  and noted in the wrapper.
- The barge unit test originally assumed node 0 was the forward half and had no damping; corrected.
- Linux build untested.
- The GitHub repository is being created by the parallel session; this tree was committed locally and not pushed.

## Next

1. Linux preset run and tolerance decision.
2. CUDA batch driver (phase 2).
3. Ship-definition schema and `shipc` (phase 3).
