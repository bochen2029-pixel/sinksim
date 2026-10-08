# Roadmap

The aim: the most physically faithful, best validated and most extensible ship flooding and sinking simulator that
can be built, with RMS Titanic as the first and best documented case. Three things carry that: real deck-level
geometry feeding one shared voxel representation; determinism and uncertainty bands for credibility; and a viewer
that renders the simulator's own water. Phases 0 and 1 are done (`docs/STATUS.md`).

| Phase | Deliverable | Acceptance | State |
|---|---|---|---|
| 0 Foundation | Repository, frozen oracle with manifest, golden trace carrying the complete state, JS self-check, formats, build wrapper | the oracle reproduces its own trace from every record | done |
| 1 C++ reference | Topology-agnostic single-source kernel, loaders with hash verification, `sinksim_run`, `sinksim_check`, `sinksim_validate`, vendored oracle-exact math | bit-exact against the golden trace, step by step and end to end; validation table reproduced | done |
| 2 CUDA batch | One block per simulation, gather-ordered flow accumulation, fixed-order reductions, mixed precision (single for column math, double partial sums), batch API over parameter vectors and action scripts | matches the CPU build at tolerance; throughput measured, not assumed | next |
| 3 Ship model v2 | Deck-level spaces digitised from the general-arrangement plans, coal bunkers, Scotland Road, gangway doors, superstructure, real hull sections voxelised; a ship-definition format where every number cites its source; the `shipc` compiler reproducing `titanic64` as its first regression test; recalibration | hydrostatics within 1 % of Harland and Wolff; Hackett and Bedford's conditions C1 to C7 reproduced | can start in parallel |
| 4 Physics | Pumps and the testimony door schedule, air compression, the list mechanisms as competing hypotheses, still-water bending moment and a break followed by two rigid bodies, forward speed for Britannic | starboard list at 11:50 and port list by 2:05 inside the bands; break time and angle inside the published range | |
| 5 Inference | Bayesian calibration on the GPU, global sensitivity, the Carpathia question as schedule optimisation over the posterior | posterior bands on trim, list and events; the Carpathia gap as an interval | |
| 6 Viewer v2 | WebAssembly build of the same kernel, voxel water rendering, deck plans shaded by water, timeline scrubber over a precomputed run, scenario editor with timed actions, ensemble bands, side-by-side comparison | single-file HTML kept; screenshot checks | |
| 7 Generality | Britannic done properly, then Lusitania, Andrea Doria, Empress of Ireland, Costa Concordia; a validation report generated on every commit; a write-up | each ship reproduces its own record | |

## Why the order

- The list cannot be fixed with parameters in the 64-node model: each compartment's upper layer is one shared port
  and starboard surface, both sides are fed symmetrically from below, and the real mechanisms (Scotland Road to port,
  cabins to starboard, the transverse bunkers, the port gangway door) are room-level topology. So the deck model
  (phase 3) comes before the list work (phase 4).
- The end game is not physical yet: the founder rule fires at a pitch of 69°. The hull-girder model, trapped air and
  buoyancy above B deck turn the last ten minutes into physics (phase 4).
- Consumer GPUs run double precision at a sixty-fourth of single rate, so the CUDA build is mixed precision by design
  (phase 2), compared with the CPU reference at tolerance rather than bit for bit.
- Calibration with more parameters than data points needs priors and bands (phase 5); every headline number should
  carry an interval before it is quoted.

## Numerics planned as flagged variants (after parity, never silently)

- A per-step capacity table per node, built in one pass and inverted by search, replacing the six Newton passes.
- A semi-implicit flow update allowing a larger step, recalibrated as its own scheme version.
- Gather-ordered accumulation on the CPU as well, so CPU and GPU can be compared bit for bit with each other.

## Decisions already locked

See `docs/adr/`. In short: one C++ source for CPU, CUDA and WebAssembly; topology is data; four separate files with
provenance (ship, scenario, parameters, actions); a determinism policy with per-step acceptance; the snapshot as the
only state boundary; uncertainty as a first-class concept; no physics in the viewer.
