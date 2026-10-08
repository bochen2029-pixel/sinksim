# Status

Updated 2026-10-08 at the end of the bootstrap session. Read this first; details in `docs/sessions/`.

## Where things stand

- Phases 0 and 1 of `docs/ROADMAP.md` are complete. The C++ reference engine reproduces the JavaScript oracle bit for
  bit over the full 1912 run and reproduces the validation table; every test passes (`tools\build\msvc.cmd all`,
  `npm test`).
- License MIT. The parent folder of this tree is the public JavaScript repository
  `github.com/bochen2029-pixel/titanic-sinking-simulator` (the oracle at its root, created by a parallel session),
  which lists `sinksim/` as an ignored companion repository. This tree is committed locally and has no remote yet;
  it can be pushed as its own repository or folded into the parent as a subdirectory, whichever the owner decides.
- Build verified on Windows (Visual Studio 2022 17.14, MSVC 19.44, CMake 4.3, Ninja). The Linux presets exist but have
  not been run yet; `std::pow` there may differ from the oracle's CRT for some inputs (see `docs/DETERMINISM.md`).

## Measured

| What | Value |
|---|---|
| Golden check, 400 dense steps, 157 windows, end to end | all differences 0 |
| Validation table against the oracle | 11 of 11 cases match |
| Engine throughput, Release, strict FP, one thread | about 25,000 steps/s; the 1912 run in 1.5 s |
| Oracle throughput, Node 24 | about 12,000 steps/s |

## Next (in order)

1. Run the Linux (WSL) preset, record whether `std::pow` matches there, and set the Linux acceptance tolerance.
2. Phase 2: the CUDA batch driver (block per simulation, gather-ordered flows, mixed precision pass functors), with
   `sinksim_check` run against the CPU trace at tolerance and a measured throughput.
3. Phase 3 groundwork, in parallel: the ship-definition schema with source citations and the `shipc` compiler that
   must reproduce `titanic64` exactly.

## Open questions

- The oracle's stale-centroid behaviour for nearly empty nodes is kept for exactness; it should become a documented
  scheme v2 change (always compute centroids, or exclude sub-tolerance volumes from the loads).
- The limiter and the six-iteration level solve are properties of the explicit scheme; the implicit variant will need
  its own calibration.
