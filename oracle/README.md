# The oracle

`js/` is the JavaScript flooding model this project started from: the headless core (`js/core/core.js`), its
scenario presets, the three.js viewer and the author's own notes (`js/README.md`, `js/PORTING.md`, `js/HANDOFF.md`).
It is frozen. The C++ engine is held to it bit for bit (docs/DETERMINISM.md), so any change here is a physics
change and goes through the procedure in `AGENTS.md`.

`MANIFEST.sha256` lists every file with its hash; `npm test` fails if anything under `js/` differs or is unlisted.
The git tag `oracle-js-verbatim` marks the package exactly as received; relative to it, two tool-level edits were made
and are the only differences:

| File | Change | Why |
|---|---|---|
| `js/core/core.js` | `hydroPass` added to the module exports (one identifier in the export list) | the golden-trace writer needs the buoyancy at the initial pose, before the first step; no computation changed |
| `js/tools/titanic.js` | the default opening area is now the calibrated value from `scenarios.js` instead of a stale 1.115 m² | the README's bare `node tools/titanic.js` produced an uncalibrated run that foundered 45 minutes early |

Everything the engine consumes is exported from this model by `tools/js/export_compiled.js`; the golden trace and the
validation table come from `tools/js/golden.js` and `tools/js/validate_oracle.js`. The viewer in `js/out/titanic.html`
still runs this JavaScript core directly and will move to the engine's WebAssembly build in a later phase.

The outputs in `js/out/` were produced by the original author on another machine and are kept for provenance; the
golden trace it contains differs from a regeneration here by one ulp in one hull coordinate (docs/DETERMINISM.md).
