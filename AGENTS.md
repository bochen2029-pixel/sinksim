# Working in this repository

This file is the canonical set of rules for any agent or person working on sinksim, in any harness. `CLAUDE.md`
points here. Read `docs/STATUS.md` first in every session: it says what exists, what passes, and what is next.

## What this is

A deterministic, time-domain ship flooding and sinking simulator. One geometry representation (vertical hull
columns, later voxels) feeds hydrostatics, flood-water capacity and rendering; a hydraulic network of spaces and
openings moves the water; a damped rigid body carries the ship. RMS Titanic (1912) is the first ship and the
calibration target; the design is ship-agnostic. `docs/ARCHITECTURE.md` has the layers, `docs/PHYSICS.md` the
equations, `docs/ROADMAP.md` the plan.

## Layers and boundaries

| Layer | Where | Rule |
|---|---|---|
| Oracle | `oracle/js` | Frozen. The original JavaScript model, hash-listed in `oracle/MANIFEST.sha256`. The engine is held to it bit for bit. |
| Ship compiler | `tools/js` now, `tools/py/shipc` later | Turns a model into compiled files (`docs/FORMATS.md`). Hashes every numeric array; the engine verifies them on load. |
| Engine | `engine/` | C++20, no external dependencies beyond `third_party/`. `engine/include/sinksim/kernel/` is single-source: plain functions over views, no allocation, no STL in the hot path, compiles as CPU, CUDA (`SS_HD`) and WebAssembly. |
| Apps and tests | `apps/`, `tests/` | Thin. Every behaviour of the engine is reachable from the command line and covered by a test. |
| Data | `ships/`, `data/` | Generated, hashed, committed. Regenerate with `npm run export`, `npm run golden`, `npm run validate:oracle`. |
| Viewer | `viewer/` (phase 6), `oracle/js/web` until then | Consumes state snapshots only. No physics in the renderer. |

Everything that crosses a boundary is a versioned file format or a plain-data struct, never a live object.

## Rules that protect correctness

1. **Never port and change physics in the same change.** A port must match the golden trace exactly before anything
   physical is touched. A physics change gets its own commit, a new golden trace, a rerun of the validation table in
   the commit message, and an ADR if the scheme or a format changes.
2. **The oracle does not change.** If it must, record the reason in an ADR, update `oracle/MANIFEST.sha256`, regenerate
   every compiled file and golden trace, and say so in `docs/DETERMINISM.md`.
3. **Determinism is a build property.** Strict floating point (`cmake/StrictFloatingPoint.cmake`), fixed-order
   reductions, no atomics in the hot path, the vendored math (`SINKSIM_MATH=v8`). Do not add `-ffast-math`, `/fp:fast`
   or contraction anywhere physics is compiled.
4. **State is explicit.** The complete dynamic state is pose, rates, volumes, levels and centroids
   (`StateSnapshot`). Anything that persists across steps belongs in it and in the trace format.
5. **Formats are versioned.** `docs/FORMATS.md` is the contract; the engine refuses unknown versions and hash
   mismatches. Extending a format means a version bump or a documented, backward-compatible optional field.
6. **Every number in a ship file will cite a source.** From phase 3 on, parameters carry provenance; see
   `docs/SOURCES.md` for the bibliography and the convention.
7. **Tests pass before a commit.** `tools/build/msvc.cmd all` on Windows (or the CMake presets elsewhere) and
   `npm test`. The golden test and the validation test are the acceptance tests of the engine.

## Conventions

- Ship frame: x forward from amidships, y to port, z up from the keel, metres. World: sea surface at Z = 0.
- Pose: heave `zO`, pitch (bow down positive), roll (starboard down positive), R = Ry(pitch) Rx(roll).
- Node index in the Titanic model: `n = (zone * 2 + side) * 2 + layer`, side 0 = port, layer 0 = below E deck.
- Tonnes = m³ × 1.025. Sources quote long tons; convert once, at the edge, with the conversion written down.
- dt = 0.25 s in scheme v1. Changing it means recalibrating.
- C++: `Real` is double, `Index` is 32-bit. Views over raw pointers in the kernel; owning containers on the host.
  Names: `snake_case` functions, `CamelCase` types, `kConstant`. Format with the repository `.clang-format`.
- Files: LF endings; `.cmd` files CRLF. No absolute machine paths in anything committed.

## Workflow

Build and test on Windows:

```
tools\build\msvc.cmd all            # configure + build + ctest for the msvc-release preset
tools\build\msvc.cmd build msvc-debug
```

Elsewhere: `cmake --preset gcc-release && cmake --build --preset gcc-release && ctest --preset gcc-release`.

Regenerate data from the oracle (Node 24): `npm run export`, `npm run golden`, `npm run perstep`, `npm run validate:oracle`.

Command-line tools after a build (`build/<preset>/bin/`): `sinksim_run`, `sinksim_check`, `sinksim_validate`; each
prints its usage when called without arguments.

## Sessions and decisions

- Start by reading `docs/STATUS.md`. End by updating it and adding `docs/sessions/YYYY-MM-DD-<slug>.md` from the
  template there: what was done, what was measured, what is next, what is unresolved.
- Decisions that are hard to reverse go in `docs/adr/NNNN-<slug>.md` (context, decision, consequences). Supersede,
  never silently edit.
- Keep `CHANGELOG.md` current for anything a user of the engine or the data would notice.
- Dependencies: nothing is fetched at build or test time. Vendor under `third_party/` with its license file and a
  line in `third_party/README.md`.

## Shell notes for agents on Windows

Write Windows paths with forward slashes in shell commands (Git Bash strips backslashes from unquoted arguments).
Author files with the editor tools rather than shell here-documents; multi-line content through a shell literal is
truncated or mangled. The build wrapper enters the Visual Studio environment itself; plain `cmake` outside it will not
find the compiler.
