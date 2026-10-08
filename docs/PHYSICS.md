# Physics of scheme v1

What the kernel computes each step, in the order it computes it. The constants of the scheme are in
`engine/include/sinksim/kernel/scheme.hpp`; everything ship-specific is data. This is the model of the original
JavaScript core, written down; its gaps and the plan to close them are in `docs/ROADMAP.md`.

## Geometry: columns and nodes

The hull is a set of vertical columns in the ship frame, each with a footprint `dxdy`, a centre `(x, y)` and a
vertical extent `[zlo, zhi]` from the hull surface to the top deck. A node is a space water can occupy; it owns a set
of column segments `[a, e]`, so that the water in the node below a world level `h` is

    V(h) = mu Σ_segments dxdy · clamp(zp − a, 0, e − a),    zp = (h − base) / R22,   base = R20 x + R21 y + tz

where `mu` is the permeability and `zp` the ship-frame height of the world plane `h` in that column. The same sum
gives the free-surface area `dV/dh = mu Σ dxdy / R22` over the segments cut by the plane, and the first moments of
the water. Buoyancy is the same integral over whole columns below `Z = 0`, with no permeability.

## Pose

Heave `zO`, pitch `θ` (bow down positive) and roll `φ` (starboard down positive); `R = Ry(θ) Rx(φ)`. The translation
places the centre of gravity `g0` at world `(XO, YO, zO)`. Surge, sway and yaw are not modelled.

## Free surfaces

Each node holds a volume; the step finds the world level at which the column integral equals that volume by a
bracketed Newton iteration (6 iterations in the step, 40 at equilibration and in diagnostics), starting from the
previous level, with the node's world extent as the bracket and bisection as the fallback. Tolerance in the step:
`max(10⁻³ Vmax, 0.5 m³)`. A node below `10⁻⁶ m³` is empty: its level is its floor and it exerts no head. A node at
`Vmax (1 − 10⁻⁷)` is full: its level rises above its top by `(V − Vmax) / Aov` with the virtual free-surface area
`Aov = max(1, 0.03 · footprint)`, which is how a full space is pressed up by water above it.

Nodes in a group share one surface: the iteration runs on the summed volume and the water is then shared in
proportion to each member's geometric volume at the common level, capped at each capacity, the last member taking the
remainder. The Titanic model pairs port and starboard halves this way; holds 2 and 3 below the firemen's tunnel stay
split until both sides stand above the tunnel top plus a margin.

A node's centroid is updated only when the solve has geometric volume above `10⁻⁹ m³`. A node holding less than the
tolerance therefore keeps an earlier centroid and still contributes to the loads with it. This is a property of the
original model, kept for exactness; the centroids are part of the state for that reason.

## Flows

Each connection joins two nodes, or the sea and a node, at an opening point whose world height is `zo`. With the
effective heads `Ha = max(ha − zo, 0)` and `Hb` likewise (an empty node has no head):

- orifice: `q = Cd A √(2g) · d / √(max(|d|, 0.01))`, `d = Ha − Hb`, which is the square-root law linearised below
  1 cm of head difference;
- weir (crest of width `w`): `q = Cw w (2/3) √(2g) H1^1.5 · f`, with `H1` the larger head, and for submerged flow the
  Villemonte factor `f = (1 − (H2/H1)^1.5)^0.385`.

Stability limiter: a flow may move at most `0.5 / deg` of the volume that would level its two ends, where `deg` is the
larger number of active connections at either end this step, and the levelling volume is
`(hs − max(hd, zo)) / (1/As + 1/Ad)` with `As`, `Ad` the effective free-surface areas (`max(area, Amin)`, or `Aov` when
full). Flows are accumulated in connection order.

Volumes are updated, clipped at zero, and capped so that no space is pressed above the sea surface: the excess goes
back to the sea and is removed from the inflow.

## Rigid body

Net vertical force and pitch and roll moments from buoyancy (acting at the centre of buoyancy in world coordinates)
and the weight of the water in every node (at its centroid), about the centre of gravity. Semi-implicit Euler for
heave, pitch and roll with linear damping `c` and quadratic damping `q`:

    a = (F − c v − q |v| v) / (M0 + M_water),

where the pitch and roll inertias include the flood water's own inertia about the centre of gravity. Pitch is clamped
at ±1.35 rad and roll at ±1.4 rad; at the clamp the rate is zeroed.

## Events

Monitors sum the positive flow through a set of connections (the overflow over each bulkhead); an event fires the
first time a monitor exceeds 0.05 m³/s. Marks are ship-frame points that fire when they first go under (or clear).
The founder rule fires when the whole hull is below the surface by 0.5 m, when |pitch| exceeds 1.2 rad, or when the
buoyant volume drops below 1 m³ after the first 10 s. In the Titanic model the run ends with the pitch runaway at about
69°, so the "foundered" time is the plunge, not a break-up (roadmap phase 4).

## Readouts

Trim and list in degrees; drafts at the perpendiculars from the world height of the keel there; flood water in m³ and
in tonnes at 1.025 t/m³; inflow in t/min.
