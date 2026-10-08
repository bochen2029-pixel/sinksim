// Plain-data views and records shared by every kernel function. A view is a bundle of raw pointers and
// counts: it owns nothing, carries no ship-specific constants, and can point into host memory, pinned
// memory or CUDA shared memory. Ownership lives on the host side (sinksim/model.hpp).
#pragma once

#include "sinksim/config.hpp"

namespace sinksim {

// ----------------------------------------------------------------------------- geometry (per ship)

// Vertical hull columns in the ship frame: the proxy from which buoyancy and flood volumes are integrated.
struct ColumnsView {
  Index n;
  const Real* x;     // m, forward from amidships
  const Real* y;     // m, to port
  const Real* dxdy;  // m2, footprint
  const Real* zlo;   // m, bottom of the column (hull)
  const Real* zhi;   // m, top of the column (top deck)
};

// A segment is the part of one column that belongs to one node (space): [a, e] in z.
struct SegmentsView {
  Index n;
  const Index* col;
  const Real* a;
  const Real* e;
};

// Nodes are the spaces water can occupy. Segments are stored CSR-style per node.
struct NodesView {
  Index n;
  const Real* mu;        // permeability
  const Real* vmax;      // m3, capacity (permeability applied)
  const Real* amin;      // m2, floor on the effective free-surface area
  const Real* aov;       // m2, virtual free-surface area once full (pressed up)
  const Index* segStart;
  const Index* segCount;
};

struct ShipView {
  ColumnsView cols;
  SegmentsView segs;
  NodesView nodes;
};

// ----------------------------------------------------------------------------- network (per simulation)

// Nodes that may share one free surface. Single: one node. Always: solved as one surface. Threshold: one
// surface once every member has water above the world height of point (px, py, pz) plus margin. Never: members
// are always solved separately (the group exists so that cross-flow connections can refer to it).
enum class GroupMode : Byte { Single = 0, Always = 1, Threshold = 2, Never = 3 };

struct GroupsView {
  Index n;
  const Index* nodeStart;
  const Index* nodeCount;
  const Index* nodes;
  const Byte* mode;
  const Real* px;
  const Real* py;
  const Real* pz;
  Real margin;
};

enum class FlowLaw : Byte { Orifice = 0, Weir = 1 };

// A connection moves water between two nodes, or between the sea (a = -1) and a node.
struct ConnectionsView {
  Index n;
  const Index* a;          // -1 = the sea
  const Index* b;
  const Byte* law;         // FlowLaw
  const Real* x;           // opening point, ship frame
  const Real* y;
  const Real* z;
  const Real* area;        // m2 for an orifice, crest width in m for a weir
  const Real* coef;        // discharge coefficient
  const Byte* kind;        // data-defined classification (doors, overflow paths, breaches, ...)
  const Index* idx;        // data-defined reference (bulkhead, zone or opening id)
  const Byte* en;          // enabled
  const Real* tOn;         // s, inactive before this time
  const Index* skipGroup;  // >= 0: inactive while that group is merged
  const Index* monitor;    // >= 0: positive flow is summed into this monitor for events
  Real* q;                 // output: m3/s through the connection this step
};

struct MonitorsView {
  Index n;
  Real threshold;  // m3/s: an overflow event fires the first time a monitor's flow exceeds this
};

enum class MarkWhen : Byte { Under = 0, Clear = 1 };

// Ship-frame points whose first submergence (or emergence) is an event.
struct MarksView {
  Index n;
  const Real* x;
  const Real* y;
  const Real* z;
  const Byte* when;  // MarkWhen
};

struct FounderRule {
  Real hullTopBelow;    // m: the whole hull envelope below the surface
  Real pitchAbsAbove;   // rad
  Real buoyancyBelow;   // m3
  Real minT;            // s: the buoyancy test only applies after this time
};

// Rigid-body constants of the loaded ship (mass properties, added mass, damping), all per simulation.
struct Body {
  Real ms;              // kg, ship mass (intact displacement)
  Real KG;              // m
  Real g0x, g0y, g0z;   // m, centre of gravity in the ship frame
  Real XO, YO;          // m, world position of the reference point
  Real mh0, Ith0, Iph0; // heave mass and pitch/roll inertia including added mass
  Real cDz, cDth, cDph; // linear damping
  Real qz, qth, qph;    // quadratic damping
};

struct Physics {
  Real rho;   // kg/m3
  Real g;     // m/s2
  Real sq2g;  // sqrt(2 g), precomputed once
};

struct ReadoutGeometry {
  Real xFP;  // m, forward perpendicular
  Real xAP;  // m, after perpendicular
};

struct SimView {
  Physics phys;
  Real dt;
  GroupsView groups;
  ConnectionsView conns;
  MonitorsView monitors;
  MarksView marks;
  FounderRule founder;
  Body body;
};

// ----------------------------------------------------------------------------- per-step records

struct Frame {
  Real R00, R01, R02, R10, R11, R12, R20, R21, R22;
  Real tx, ty, tz;
  Real cth;
};

struct Buoyancy {
  Real V;        // m3
  Real x, y, z;  // m, centre of buoyancy, ship frame
  Real top;      // m, highest point of the hull envelope in world Z
};

struct NodePass {
  Real vol;   // m3, water below the level (permeability applied)
  Real dv;    // m2, free-surface area (dV/dh at the level)
  Real mx, my, mz;
  Real zmin, zmax;  // m, world extent of the node
};

struct Loads {
  Real Fz, Mth, Mph;
  Real mw, Iw_p, Iw_r;
  Real bxw, byw;
};

// ----------------------------------------------------------------------------- state

// Everything the next step depends on. vol and level have one entry per node.
struct State {
  Real t, zO, pitch, roll;
  Real vz, wth, wph;
  Real* vol;
  Real* level;
};

// Recomputed every step; kept because readouts, events and the viewer use them.
struct Scratch {
  Real* area;       // per node: free-surface area at the solved level
  Real* cx;         // per node: water centroid, ship frame
  Real* cy;
  Real* cz;
  Real* zmin;       // per node: world extent
  Real* zmax;
  Real* hEff;       // per node: effective head (kNoHead when empty)
  Real* aEff;       // per node: effective free-surface area for the limiter
  Real* acc;        // per node: volume change this step
  Index* deg;       // per node: active connections this step
  Byte* merged;     // per group
  NodePass* passes; // scratch for group solves, >= largest group
  Real* over;       // per monitor
};

struct Outputs {
  Real inflow;   // m3/s entering through all sea connections
  Real Vb;       // m3, buoyant volume at the start of the step
  Real hullTop;  // m, world Z of the highest hull point
  Real mw;       // kg, flood water mass
  Real bx, by, bz;
};

enum class EventKind : Byte { Overflow = 0, Mark = 1, Founder = 2 };

struct Event {
  Real t;
  Index id;   // monitor or mark index; unused for Founder
  EventKind kind;
};

struct EventLog {
  Event* items;
  Index capacity;
  Index count;
  Real* overT;   // per monitor: time of the first overflow, -1 before
  Real* markT;   // per mark
  Byte foundered;
  Real founderT;
};

SS_HD inline void push_event(EventLog& ev, Real t, Index id, EventKind kind) {
  if (ev.count < ev.capacity) {
    ev.items[ev.count].t = t;
    ev.items[ev.count].id = id;
    ev.items[ev.count].kind = kind;
    ev.count++;
  }
}

}  // namespace sinksim
