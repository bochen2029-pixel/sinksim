# Changelog

## 0.3.0 (2026-10-08)

- Mixed precision: single-precision column arithmetic and partial sums in the GPU's reduction order, as the
  `portable32` numerics on the CPU and `--precision mixed` (the default) on the GPU; its own reference trace; the
  GPU matches the CPU exactly in both precisions. 2.4 times the double-precision throughput.
- Sweeps: `sinksim.sweep` and `sinksim.batch-results` formats, `sinksim_sweep` (threaded CPU runner and exact
  comparison), `sinksim_cuda_run --sweep` with readout curves recorded on the device, CTest holding the GPU's sweep
  equal to the CPU's.
- Observation targets and the oracle's calibration objective exported as data; `tools/py/sweep.py` and
  `tools/py/calibrate_gpu.py`.
- ADR 0007; documentation of the measurements.

## 0.2.0 (2026-10-08)

- Portable numerics: fdlibm's sin, cos, pow and scalbn as host-and-device functions, the canonical reduction tree
  with CPU emulation, and `Numerics` (oracle, portable, std) chosen per simulation; `--numerics` on every tool.
- The flow phase split into per-node and per-connection pieces with an incidence gather (same bits as before).
- The portable reference trace and its test; the oracle and portable checks pass exactly on Windows and Linux.
- CUDA batch engine (`SINKSIM_ENABLE_CUDA`, preset `msvc-cuda-release`): one block per simulation, chunked launches,
  per-instance connection fields and state, traces; `sinksim_cuda_run`; tests holding the GPU bit for bit to the CPU.
- Static MSVC runtime; language-guarded strict floating-point flags; ADR 0006.

## 0.1.0 (2026-10-08)

First public version.

- Frozen JavaScript oracle with manifest and two tool-level fixes (hydroPass export, calibrated default area in
  `tools/titanic.js`).
- Compiled file formats (`sinksim.compiled-ship`, `sinksim.compiled-sim`, `sinksim.catalog`, `sinksim.trace`,
  `sinksim.validation`), version 1, with FNV-1a hashes on every numeric array.
- Node tools that export the Titanic model and its 11 scenarios, write golden traces carrying the complete state
  (centroids included), prove the oracle reproduces itself, and produce the validation table.
- C++20 reference engine: single-source kernel, host model, loaders with hash verification, trace IO, `Simulation`,
  `sinksim_run`, `sinksim_check`, `sinksim_validate`.
- Vendored oracle-exact math (fdlibm sin and cos, V8 pow semantics) selectable with `SINKSIM_MATH`; nlohmann JSON.
- Tests: hash vectors, JSON layer, kernel on a synthetic barge, the golden check at tolerance zero, the validation
  comparison; JS test enforcing the oracle manifest.
- Documentation: AGENTS, architecture, physics, formats, determinism, validation, roadmap, sources, ADRs 0001 to 0005.
