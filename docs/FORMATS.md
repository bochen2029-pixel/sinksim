# File formats

All formats are JSON with a `format` string and an integer `formatVersion`. Numeric arrays are written as the
shortest decimal that round-trips the double (JavaScript's `JSON.stringify`, the engine's `std::to_chars`), and every
numeric array is hashed; the engine refuses a file whose arrays do not hash to the values the writer recorded. Objects
are written one member per line and arrays on one line, so files from the JS tools and from the engine look alike.

Units: metres, seconds, kilograms, radians, cubic metres. Frames: `AGENTS.md`.

## Hashing

FNV-1a, 64 bit, over the little-endian bytes of each array: doubles as 8 bytes, 32-bit indices as 4 bytes, byte
flags as 1 byte. Negative zero is normalised to zero before hashing (JSON cannot distinguish them). Each file carries
`hash.<array name>` per array and `hash.all`, the same function run over every array in the documented order. The
implementations are `tools/js/lib/fnv.js` and `engine/include/sinksim/hash.hpp`; `tests/test_hash.cpp` holds the
published test vectors.

## `sinksim.compiled-ship` (version 1)

The geometry of one ship model, shared by every simulation of it. File: `ships/<ship>/<id>.ship.json`.

| Member | Content |
|---|---|
| `id`, `meta` | model id; title, generator, engine that produced it, oracle hashes, creation time |
| `units`, `frame` | documentation of the conventions |
| `columns` | `n`, `dy`, and arrays `x`, `y`, `dxdy`, `dx`, `zlo`, `zhi` (f64), `zone`, `side` (i32): vertical hull columns in the ship frame |
| `nodes` | `n`, `label[]`, `zone[]`, `side[]`, `layer[]`, and `mu`, `vmax`, `amin`, `aov` (f64), `segStart`, `segCount` (i32): the spaces water can occupy, with their segments in CSR layout |
| `segments` | `n`, `col` (i32), `a`, `e` (f64): the part of column `col` between heights `a` and `e` that belongs to a node |
| `zones`, `hydrostatics` | informational: compartment table and the intact hydrostatics |
| `hash` | per array and `all` |

Hash order: `columns.x y dxdy zlo zhi zone side`, `nodes.mu vmax amin aov segStart segCount`, `segments.col a e`.

## `sinksim.compiled-sim` (version 1)

One scenario on a ship: the network, constants and initial state. File: `ships/<ship>/sims/<id>.sim.json`.

| Member | Content |
|---|---|
| `id`, `ship`, `shipHash`, `meta` | ids; `shipHash` must equal the ship file's `hash.all` |
| `constants` | `rho`, `g` |
| `scheme` | `dt` |
| `params` | the oracle's parameter object, for reference (everything the kernel needs is already baked into the arrays) |
| `groups` | `n`, `nodeStart`, `nodeCount`, `nodes` (i32), `mode` (u8: 0 single, 1 always one surface, 2 one surface once every member is above the world height of `(px, py, pz)` plus `margin`, 3 never), `px`, `py`, `pz` (f64), `margin` |
| `connections` | `n`, `a`, `b` (i32; `a = -1` is the sea), `type` (u8: 0 orifice, 1 weir), `x`, `y`, `z` (opening point), `area` (m² for an orifice, crest width in m for a weir), `coef`, `kind` (u8, names in `kindNames`), `idx` (i32), `en` (u8), `tOn` (f64), `skipGroup` (i32, inactive while that group is merged), `monitor` (i32, flows into this event monitor) |
| `monitors` | `n`, `threshold` (m³/s), `id[]`, `label[]` |
| `marks` | `n`, `id[]`, `label[]`, `x`, `y`, `z` (f64), `when` (u8: 0 fires when the point goes under, 1 when it clears) |
| `founder` | `hullTopBelow`, `pitchAbsAbove`, `buoyancyBelow`, `minT` |
| `body` | `ms`, `KG`, `g0[3]`, `XO`, `YO`, `mh0`, `Ith0`, `Iph0`, `cDz`, `cDth`, `cDph`, `qz`, `qth`, `qph` |
| `readout` | `xFP`, `xAP` |
| `state0` | `t`, `zO`, `pitch`, `roll`, `vz`, `wth`, `wph`, and per node `vol`, `level`, `cx`, `cy`, `cz` |
| `openings`, `doors` | informational: the openings with the connection each became, the doors and bulkhead tops |
| `hash` | per array and `all` |

Hash order: `groups.nodeStart nodeCount nodes mode px py pz`, `connections.a b type x y z area coef kind idx en tOn
skipGroup monitor`, `marks.x y z when`, `state0.vol level cx cy cz`.

## `sinksim.catalog` (version 1)

`ships/<ship>/catalog.json`: `ship`, `shipFile`, and `cases[]` of `{id, label, file, tMaxH, keepHist}` with paths
relative to the catalog. `sinksim_validate` runs every case with the oracle's options (history every 30 s, stop when
stable).

## `sinksim.trace` (version 1)

The complete dynamic state along a run. Files: `data/golden/<ship>/<sim>.trace.json` (role `golden`, written by the
oracle) and whatever `sinksim_run --trace` writes.

| Member | Content |
|---|---|
| `role`, `ship`, `sim`, `meta` | `golden` or `trace`; producer and engine strings |
| `dt`, `nodes`, `denseSteps`, `sampleEvery`, `steps`, `tEnd`, `foundered`, `founderT` | run summary |
| `events[]` | `{t, id, label}` in the order they fired |
| `dense`, `samples` | columnar records: `n` and arrays `t`, `zO`, `pitch`, `roll`, `vz`, `wth`, `wph`, `inflow`, `Vb`, `hullTop` (one per record) and `vol`, `level`, `cx`, `cy`, `cz` (flat, `n × nodes`). `dense` holds the initial record and every step up to `denseSteps`; `samples` holds the initial record and one every `sampleEvery` seconds. |
| `hash` | per array and `all`, over `dense` then `samples`, fields in the order above |

The record is complete: restarting a simulation from any record and stepping reproduces the next record exactly.
The centroids are part of it because the oracle keeps a node's last computed centroid while the node holds less
water than the solver tolerance and still uses it in the loads (docs/DETERMINISM.md).

## `sinksim.sweep` (version 1)

Many instances of one compiled simulation that differ by connection changes, for `sinksim_sweep` (CPU) and
`sinksim_cuda_run --sweep` (GPU). Written by hand or by `tools/py/sweep.py`.

| Member | Content |
|---|---|
| `ship`, `sim` | informational; the tools take the compiled files on the command line |
| `tMax`, `every` | s; the run length and the curve interval (the oracle's run history uses 30 s) |
| `stopWhenStable` | the oracle's calm rule; CPU only |
| `instances[]` | `id`; `scale`: an object of connection kind name to factor, multiplying the compiled area of every connection of that kind (the four calibration parameters of the Titanic model are the kinds `breach`, `over`, `down`, `top`); `connections[]`: overrides selecting one connection (`conn`) or every connection of a `kind` (and `idx`), setting any of `enabled`, `tOn` (s) and `area` |

Overrides apply after scales, in the order written.

## `sinksim.batch-results` (version 1)

What a sweep produced: `ship`, `sim`, `numerics`, `engine`, `tMax`, `every`, and `instances[]` with `id`, `foundered`,
`founderT`, `tEnd`, `steps`, `final` (`trimDeg`, `listDeg`, `waterT`, `draftF`, `draftA`, `inflowTpm`), `events[]`
(`t`, `id`, `label`) and `curve` (columns `t`, `trim`, `list`, `water`, `inflow`, `draftF`, `draftA`: the readouts
every `every` seconds from the initial state, closed by a record at the end of the run, exactly the oracle's run
history). `sinksim_sweep --compare a b` compares two files value for value; the GPU's results equal the CPU's in
the same numerics.

## `sinksim.observations` (version 1)

`data/observations/titanic-1912.json`, exported from the oracle by `tools/js/export_observations.js`: Halpern's
eyewitness-derived trim and list against time, the foundering and break-up times, the timeline events, and the
weights of the oracle's calibration objective written down so that any driver reproduces it (`tools/py/calibrate_gpu.py`).

## `sinksim.validation` (version 1)

`data/validation/<ship>/validation.oracle.json` and the engine's `validation.engine.json`: `cases` keyed by id with
`label`, `foundered`, `founderT`, `founderMin`, `endT`, `trimDeg`, `listDeg`, `waterT`, `events[]`, and for selected
cases `hist` (minutes, trim, list, tonnes) as the original viewer expects.
