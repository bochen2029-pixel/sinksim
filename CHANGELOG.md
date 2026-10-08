# Changelog

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
