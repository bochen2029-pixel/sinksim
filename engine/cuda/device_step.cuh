// The step on the device: one thread block per simulation, state and scratch in shared memory, the same kernel
// functions as the CPU. Only the two column integrals and the warp passes use cooperative reductions, and they
// follow the canonical tree of kernel/reduce.hpp, which the CPU's portable numerics emulate; every other phase
// is the serial code run by one thread or spread over threads without changing any summation order.
#pragma once

#include <cuda_runtime.h>

#include "sinksim/kernel/body.hpp"
#include "sinksim/kernel/events.hpp"
#include "sinksim/kernel/flows.hpp"
#include "sinksim/kernel/frame.hpp"
#include "sinksim/kernel/hydro.hpp"
#include "sinksim/kernel/levels.hpp"
#include "sinksim/kernel/math.hpp"
#include "sinksim/kernel/readouts.hpp"
#include "sinksim/kernel/reduce.hpp"
#include "sinksim/kernel/types.hpp"

namespace sinksim::cuda {

using namespace sinksim::kernel;

constexpr unsigned kFullMask = 0xffffffffu;

// The immutable part of a simulation, shared by every instance of the batch (device pointers).
struct DevSim {
  Physics phys;
  Real dt;
  GroupsView groups;
  Index nConn;
  const Index* a; const Index* b; const Byte* law;
  const Real* x; const Real* y; const Real* z; const Real* coef;
  const Byte* kind; const Index* idx; const Index* skipGroup; const Index* monitor;
  IncidenceView inc;
  MonitorsView monitors;
  MarksView marks;
  FounderRule founder;
  Body body;
  ReadoutGeometry readout;
  Index nNodes, nGroups, maxGroup, nMon, nMarks, evCap;
};

// Per-instance arrays, [count * stride].
struct DevBatch {
  Index count;
  Byte* en; Real* area; Real* tOn;                                  // count * nConn
  Real* t; Real* zO; Real* pitch; Real* roll; Real* vz; Real* wth; Real* wph;  // count
  Real* vol; Real* level; Real* cx; Real* cy; Real* cz;             // count * nNodes
  Real* inflow; Real* Vb; Real* hullTop; Real* mw; Real* bx; Real* by; Real* bz;  // count
  Event* evItems; Index* evCount; Real* overT; Real* markT; Byte* foundered; Real* founderT;  // count * evCap, count, count * nMon, count * nMarks, count, count
  long long* steps;                                                 // count
  // tracing of the first `traced` instances
  int traced; int denseSteps; Real sampleEvery; int maxSamples; int recSize;
  Real* denseRec; Real* sampleRec;                                  // traced * (denseSteps + 1) * recSize, traced * maxSamples * recSize
  int* denseCount; int* sampleCount; Real* nextSample;              // traced
  // readout curve of every instance (the oracle's run history): 7 values per record, recorded when t >= nextCurve
  Real curveEvery; int curveMax;                                    // curveEvery <= 0 disables
  Real* curveRec;                                                   // count * curveMax * 7
  int* curveCount; Real* nextCurve;                                 // count
};

constexpr int kCurveValues = 7;

// Byte offsets of everything that lives in shared memory for one block.
struct SmemLayout {
  std::size_t vol, level, cx, cy, cz, area, zmin, zmax, hEff, aEff, acc, excess;
  std::size_t q, dV, over, passes, warpHydro, state, outputs, frame, hb, evItems, overT, markT, scalars;
  std::size_t deg, merged, evlog;
  std::size_t total;
};

inline std::size_t align8(std::size_t x) { return (x + 7) & ~static_cast<std::size_t>(7); }

inline SmemLayout smem_layout(Index nNodes, Index nConn, Index nGroups, Index nMon, Index nMarks, Index maxGroup, Index evCap) {
  SmemLayout L{};
  std::size_t off = 0;
  auto take = [&](std::size_t bytes) { const std::size_t at = off; off = align8(off + bytes); return at; };
  const std::size_t nodeBytes = sizeof(Real) * static_cast<std::size_t>(nNodes);
  L.vol = take(nodeBytes); L.level = take(nodeBytes); L.cx = take(nodeBytes); L.cy = take(nodeBytes); L.cz = take(nodeBytes);
  L.area = take(nodeBytes); L.zmin = take(nodeBytes); L.zmax = take(nodeBytes); L.hEff = take(nodeBytes); L.aEff = take(nodeBytes);
  L.acc = take(nodeBytes); L.excess = take(nodeBytes);
  L.q = take(sizeof(Real) * static_cast<std::size_t>(nConn));
  L.dV = take(sizeof(Real) * static_cast<std::size_t>(nConn));
  L.over = take(sizeof(Real) * static_cast<std::size_t>(nMon > 0 ? nMon : 1));
  L.passes = take(sizeof(NodePass) * static_cast<std::size_t>(kBlockWarps) * static_cast<std::size_t>(maxGroup));
  L.warpHydro = take(sizeof(HydroPartial) * static_cast<std::size_t>(kBlockWarps));
  L.state = take(sizeof(State));
  L.outputs = take(sizeof(Outputs));
  L.frame = take(sizeof(Frame));
  L.hb = take(sizeof(Buoyancy));
  L.evItems = take(sizeof(Event) * static_cast<std::size_t>(evCap));
  L.overT = take(sizeof(Real) * static_cast<std::size_t>(nMon > 0 ? nMon : 1));
  L.markT = take(sizeof(Real) * static_cast<std::size_t>(nMarks > 0 ? nMarks : 1));
  L.scalars = take(sizeof(Real) * 8);
  L.deg = take(sizeof(Index) * static_cast<std::size_t>(nNodes));
  L.merged = take(sizeof(Byte) * static_cast<std::size_t>(nGroups));
  L.evlog = take(sizeof(EventLog));
  L.total = off;
  return L;
}

template <class T>
__device__ inline T* carve(unsigned char* base, std::size_t off) { return reinterpret_cast<T*>(base + off); }

// Warp tree over a HydroPartial: offsets 16..1, lane l < off absorbs lane l + off; the result is in lane 0.
__device__ inline void warp_tree(HydroPartial& p, int lane) {
  for (int off = kWarpLanes / 2; off > 0; off >>= 1) {
    HydroPartial o;
    o.V = __shfl_down_sync(kFullMask, p.V, off);
    o.Mx = __shfl_down_sync(kFullMask, p.Mx, off);
    o.My = __shfl_down_sync(kFullMask, p.My, off);
    o.Mz = __shfl_down_sync(kFullMask, p.Mz, off);
    o.top = __shfl_down_sync(kFullMask, p.top, off);
    if (lane < off) combine(p, o);
  }
}

__device__ inline void warp_tree(PassPartial& p, int lane) {
  for (int off = kWarpLanes / 2; off > 0; off >>= 1) {
    PassPartial o;
    o.vol = __shfl_down_sync(kFullMask, p.vol, off);
    o.dv = __shfl_down_sync(kFullMask, p.dv, off);
    o.mx = __shfl_down_sync(kFullMask, p.mx, off);
    o.my = __shfl_down_sync(kFullMask, p.my, off);
    o.mz = __shfl_down_sync(kFullMask, p.mz, off);
    o.zmin = __shfl_down_sync(kFullMask, p.zmin, off);
    o.zmax = __shfl_down_sync(kFullMask, p.zmax, off);
    if (lane < off) combine(p, o);
  }
}

__device__ inline void warp_broadcast(PassPartial& p) {
  p.vol = __shfl_sync(kFullMask, p.vol, 0);
  p.dv = __shfl_sync(kFullMask, p.dv, 0);
  p.mx = __shfl_sync(kFullMask, p.mx, 0);
  p.my = __shfl_sync(kFullMask, p.my, 0);
  p.mz = __shfl_sync(kFullMask, p.mz, 0);
  p.zmin = __shfl_sync(kFullMask, p.zmin, 0);
  p.zmax = __shfl_sync(kFullMask, p.zmax, 0);
}

// Single-precision counterparts (mixed precision, reduce.hpp).
__device__ inline void warp_tree(HydroPartialF& p, int lane) {
  for (int off = kWarpLanes / 2; off > 0; off >>= 1) {
    HydroPartialF o;
    o.V = __shfl_down_sync(kFullMask, p.V, off);
    o.Mx = __shfl_down_sync(kFullMask, p.Mx, off);
    o.My = __shfl_down_sync(kFullMask, p.My, off);
    o.Mz = __shfl_down_sync(kFullMask, p.Mz, off);
    o.top = __shfl_down_sync(kFullMask, p.top, off);
    if (lane < off) combine(p, o);
  }
}

__device__ inline void warp_tree(PassPartialF& p, int lane) {
  for (int off = kWarpLanes / 2; off > 0; off >>= 1) {
    PassPartialF o;
    o.vol = __shfl_down_sync(kFullMask, p.vol, off);
    o.dv = __shfl_down_sync(kFullMask, p.dv, off);
    o.mx = __shfl_down_sync(kFullMask, p.mx, off);
    o.my = __shfl_down_sync(kFullMask, p.my, off);
    o.mz = __shfl_down_sync(kFullMask, p.mz, off);
    o.zmin = __shfl_down_sync(kFullMask, p.zmin, off);
    o.zmax = __shfl_down_sync(kFullMask, p.zmax, off);
    if (lane < off) combine(p, o);
  }
}

__device__ inline void warp_broadcast(PassPartialF& p) {
  p.vol = __shfl_sync(kFullMask, p.vol, 0);
  p.dv = __shfl_sync(kFullMask, p.dv, 0);
  p.mx = __shfl_sync(kFullMask, p.mx, 0);
  p.my = __shfl_sync(kFullMask, p.my, 0);
  p.mz = __shfl_sync(kFullMask, p.mz, 0);
  p.zmin = __shfl_sync(kFullMask, p.zmin, 0);
  p.zmax = __shfl_sync(kFullMask, p.zmax, 0);
}

// The node pass done by one warp: lane partials over strided segments, the tree, the result in every lane.
struct WarpPasser {
  const ShipView& S;
  const Frame& F;
  int lane;
  __device__ NodePass operator()(Index n, Real h) const {
    const Index s0 = S.nodes.segStart[n], s1 = s0 + S.nodes.segCount[n];
    const Real iR = 1 / F.R22;
    PassPartial p = pass_zero();
    for (Index j = s0 + lane; j < s1; j += kWarpLanes) pass_segment(S, F, iR, h, j, p);
    warp_tree(p, lane);
    warp_broadcast(p);
    return finish_pass(S, n, iR, p);
  }
};

// The same in mixed precision: single-precision lane partials and tree, the finished totals in double.
struct WarpPasserMixed {
  const ShipView& S;
  const Frame& F;
  FrameF f;
  int lane;
  __device__ WarpPasserMixed(const ShipView& S_, const Frame& F_, int lane_) : S(S_), F(F_), f(frame_f(F_)), lane(lane_) {}
  __device__ NodePass operator()(Index n, Real h) const {
    const Index s0 = S.nodes.segStart[n], s1 = s0 + S.nodes.segCount[n];
    const float hf = static_cast<float>(h);
    PassPartialF p = pass_zero_f();
    for (Index j = s0 + lane; j < s1; j += kWarpLanes) pass_segment_f(S, f, hf, j, p);
    warp_tree(p, lane);
    warp_broadcast(p);
    return finish_pass(S, n, 1 / F.R22, widen(p));
  }
};

// Everything a block needs, resolved once per launch.
struct BlockContext {
  ShipView ship;
  SimView M;
  State* st;
  Scratch sc;
  Outputs* out;
  Frame* F;
  Buoyancy* hb;
  HydroPartial* warpHydro;
  Real* scalars;
  EventLog* ev;
  NodePass* passesBase;
  Index maxGroup;
};

// Pose and buoyancy at the current state: thread 0 sets the frame, every thread sums columns, the warps and
// then thread 0 combine in the canonical order. In mixed precision the thread partials and the warp trees are
// single precision and the warp results are widened before thread 0 combines them, as the CPU emulation does.
template <bool Mixed>
__device__ inline void block_pose_and_hydro(BlockContext& C) {
  const int tid = threadIdx.x, warp = tid / kWarpLanes, lane = tid % kWarpLanes;
  if (tid == 0) *C.F = pose_frame<PortableMath>(C.M.body, C.st->zO, C.st->pitch, C.st->roll);
  __syncthreads();
  if constexpr (Mixed) {
    const FrameF f = frame_f(*C.F);
    HydroPartialF p = hydro_zero_f();
    for (Index i = tid; i < C.ship.cols.n; i += kBlockThreads) hydro_column_f(C.ship.cols, f, i, p);
    warp_tree(p, lane);
    if (lane == 0) C.warpHydro[warp] = widen(p);
  } else {
    HydroPartial p = hydro_zero();
    const Real iR = 1 / C.F->R22;
    for (Index i = tid; i < C.ship.cols.n; i += kBlockThreads) hydro_column(C.ship.cols, *C.F, iR, i, p);
    warp_tree(p, lane);
    if (lane == 0) C.warpHydro[warp] = p;
  }
  __syncthreads();
  if (tid == 0) {
    HydroPartial total = C.warpHydro[0];
    for (int w = 1; w < kBlockWarps; ++w) combine(total, C.warpHydro[w]);
    *C.hb = finish_hydro(total);
  }
  __syncthreads();
}

template <bool Mixed>
__device__ inline void device_step(BlockContext& C) {
  const int tid = threadIdx.x, warp = tid / kWarpLanes, lane = tid % kWarpLanes;
  const Index NN = C.ship.nodes.n, NC = C.M.conns.n, NG = C.M.groups.n;
  State& st = *C.st;
  Outputs& out = *C.out;

  block_pose_and_hydro<Mixed>(C);
  const Frame& F = *C.F;

  // free surfaces: one warp per group
  {
    Scratch scw = C.sc;
    scw.passes = C.passesBase + warp * C.maxGroup;
    if constexpr (Mixed) {
      const WarpPasserMixed pass(C.ship, F, lane);
      for (Index g = warp; g < NG; g += kBlockWarps) solve_group_entry(C.ship.nodes, C.M.groups, F, pass, false, st, scw, g);
    } else {
      const WarpPasser pass{C.ship, F, lane};
      for (Index g = warp; g < NG; g += kBlockWarps) solve_group_entry(C.ship.nodes, C.M.groups, F, pass, false, st, scw, g);
    }
  }
  __syncthreads();

  for (Index n = tid; n < NN; n += kBlockThreads) effective_heads(C.ship, st, C.sc, n);
  __syncthreads();
  for (Index n = tid; n < NN; n += kBlockThreads) node_degree(C.M, C.sc, n);
  __syncthreads();
  for (Index c = tid; c < NC; c += kBlockThreads) C.sc.dV[c] = connection_flow<PortableMath>(C.M, F, st, C.sc, c);
  __syncthreads();
  for (Index n = tid; n < NN; n += kBlockThreads) node_accumulate(C.M, C.sc, n);
  if (tid == 0) C.scalars[0] = sea_inflow(C.M, C.sc);
  __syncthreads();
  for (Index n = tid; n < NN; n += kBlockThreads) node_volume_update(C.ship, st, C.sc, n);
  __syncthreads();
  if (tid == 0) {
    const Real seaIn = return_excess(C.ship, C.sc, C.scalars[0]);
    out.inflow = seaIn / C.M.dt;
    const Loads L = loads(C.ship, C.M, F, *C.hb, st, C.sc);
    integrate(C.M, L, st);
    out.Vb = C.hb->V; out.hullTop = C.hb->top; out.mw = L.mw;
    out.bx = C.hb->x; out.by = C.hb->y; out.bz = C.hb->z;
    track_events(C.M, F, st, out, C.sc, *C.ev);
  }
  __syncthreads();
}

// One record of the complete state into a trace buffer (thread 0).
__device__ inline void write_record(Real* rec, const State& st, const Outputs& out, const Scratch& sc, Index NN) {
  rec[0] = st.t; rec[1] = st.zO; rec[2] = st.pitch; rec[3] = st.roll; rec[4] = st.vz; rec[5] = st.wth; rec[6] = st.wph;
  rec[7] = out.inflow; rec[8] = out.Vb; rec[9] = out.hullTop;
  Real* p = rec + 10;
  for (Index n = 0; n < NN; ++n) p[n] = st.vol[n];
  p += NN;
  for (Index n = 0; n < NN; ++n) p[n] = st.level[n];
  p += NN;
  for (Index n = 0; n < NN; ++n) p[n] = sc.cx[n];
  p += NN;
  for (Index n = 0; n < NN; ++n) p[n] = sc.cy[n];
  p += NN;
  for (Index n = 0; n < NN; ++n) p[n] = sc.cz[n];
}

template <bool Mixed>
__global__ void __launch_bounds__(kBlockThreads) run_chunk(ShipView ship, DevSim sim, DevBatch batch, SmemLayout L, long long maxSteps, Real tMax, int firstLaunch) {
  extern __shared__ unsigned char smem[];
  const int inst = blockIdx.x;
  const int tid = threadIdx.x;
  const Index NN = sim.nNodes, NC = sim.nConn;
  const std::size_t instN = static_cast<std::size_t>(inst) * static_cast<std::size_t>(NN);
  const std::size_t instC = static_cast<std::size_t>(inst) * static_cast<std::size_t>(NC);

  BlockContext C;
  C.ship = ship;
  C.M.phys = sim.phys; C.M.dt = sim.dt; C.M.groups = sim.groups;
  C.M.conns = ConnectionsView{NC, sim.a, sim.b, sim.law, sim.x, sim.y, sim.z, batch.area + instC, sim.coef, sim.kind, sim.idx,
                              batch.en + instC, batch.tOn + instC, sim.skipGroup, sim.monitor, carve<Real>(smem, L.q)};
  C.M.inc = sim.inc; C.M.monitors = sim.monitors; C.M.marks = sim.marks; C.M.founder = sim.founder; C.M.body = sim.body;
  C.st = carve<State>(smem, L.state);
  C.out = carve<Outputs>(smem, L.outputs);
  C.F = carve<Frame>(smem, L.frame);
  C.hb = carve<Buoyancy>(smem, L.hb);
  C.warpHydro = carve<HydroPartial>(smem, L.warpHydro);
  C.scalars = carve<Real>(smem, L.scalars);
  C.ev = carve<EventLog>(smem, L.evlog);
  C.passesBase = carve<NodePass>(smem, L.passes);
  C.maxGroup = sim.maxGroup;
  C.sc = Scratch{carve<Real>(smem, L.area), carve<Real>(smem, L.cx), carve<Real>(smem, L.cy), carve<Real>(smem, L.cz),
                 carve<Real>(smem, L.zmin), carve<Real>(smem, L.zmax), carve<Real>(smem, L.hEff), carve<Real>(smem, L.aEff),
                 carve<Real>(smem, L.acc), carve<Real>(smem, L.excess), carve<Index>(smem, L.deg), carve<Byte>(smem, L.merged),
                 C.passesBase, carve<Real>(smem, L.over), carve<Real>(smem, L.dV)};
  Real* vol = carve<Real>(smem, L.vol);
  Real* level = carve<Real>(smem, L.level);
  Event* evItems = carve<Event>(smem, L.evItems);
  Real* overT = carve<Real>(smem, L.overT);
  Real* markT = carve<Real>(smem, L.markT);

  // load the instance
  if (tid == 0) {
    C.st->t = batch.t[inst]; C.st->zO = batch.zO[inst]; C.st->pitch = batch.pitch[inst]; C.st->roll = batch.roll[inst];
    C.st->vz = batch.vz[inst]; C.st->wth = batch.wth[inst]; C.st->wph = batch.wph[inst];
    C.st->vol = vol; C.st->level = level;
    C.out->inflow = batch.inflow[inst]; C.out->Vb = batch.Vb[inst]; C.out->hullTop = batch.hullTop[inst]; C.out->mw = batch.mw[inst];
    C.out->bx = batch.bx[inst]; C.out->by = batch.by[inst]; C.out->bz = batch.bz[inst];
    C.ev->items = evItems; C.ev->capacity = sim.evCap; C.ev->count = batch.evCount[inst];
    C.ev->overT = overT; C.ev->markT = markT; C.ev->foundered = batch.foundered[inst]; C.ev->founderT = batch.founderT[inst];
  }
  for (Index n = tid; n < NN; n += kBlockThreads) {
    vol[n] = batch.vol[instN + n]; level[n] = batch.level[instN + n];
    C.sc.cx[n] = batch.cx[instN + n]; C.sc.cy[n] = batch.cy[instN + n]; C.sc.cz[n] = batch.cz[instN + n];
    C.sc.area[n] = 0; C.sc.zmin[n] = 0; C.sc.zmax[n] = 0; C.sc.hEff[n] = 0; C.sc.aEff[n] = 0; C.sc.acc[n] = 0; C.sc.excess[n] = 0; C.sc.deg[n] = 0;
  }
  for (Index c = tid; c < NC; c += kBlockThreads) { C.sc.dV[c] = 0; C.M.conns.q[c] = 0; }
  for (Index g = tid; g < sim.nGroups; g += kBlockThreads) C.sc.merged[g] = 0;
  for (Index e = tid; e < sim.evCap; e += kBlockThreads) evItems[e] = batch.evItems[static_cast<std::size_t>(inst) * sim.evCap + e];
  for (Index m = tid; m < sim.nMon; m += kBlockThreads) overT[m] = batch.overT[static_cast<std::size_t>(inst) * sim.nMon + m];
  for (Index m = tid; m < sim.nMarks; m += kBlockThreads) markT[m] = batch.markT[static_cast<std::size_t>(inst) * sim.nMarks + m];
  __syncthreads();

  const bool traced = inst < batch.traced;
  long long done = batch.steps[inst];
  if (firstLaunch && traced) {
    // the initial record: buoyancy at the initial pose, no inflow yet
    block_pose_and_hydro<Mixed>(C);
    if (tid == 0) {
      C.out->inflow = 0; C.out->Vb = C.hb->V; C.out->hullTop = C.hb->top; C.out->bx = C.hb->x; C.out->by = C.hb->y; C.out->bz = C.hb->z;
      write_record(batch.denseRec + static_cast<std::size_t>(inst) * (batch.denseSteps + 1) * batch.recSize, *C.st, *C.out, C.sc, NN);
      write_record(batch.sampleRec + static_cast<std::size_t>(inst) * batch.maxSamples * batch.recSize, *C.st, *C.out, C.sc, NN);
      batch.denseCount[inst] = 1; batch.sampleCount[inst] = 1; batch.nextSample[inst] = batch.sampleEvery;
    }
    __syncthreads();
  }

  for (long long s = 0; s < maxSteps; ++s) {
    if (C.ev->foundered || C.st->t >= tMax) break;   // uniform: shared values read by every thread
    if (batch.curveEvery > 0 && tid == 0 && C.st->t >= batch.nextCurve[inst] && batch.curveCount[inst] < batch.curveMax) {
      const Readouts r = readouts<PortableMath>(C.ship, C.M, sim.readout, *C.st, *C.out);
      Real* rec = batch.curveRec + (static_cast<std::size_t>(inst) * batch.curveMax + batch.curveCount[inst]) * kCurveValues;
      rec[0] = r.t; rec[1] = r.trimDeg; rec[2] = r.listDeg; rec[3] = r.waterT; rec[4] = r.inflowTpm; rec[5] = r.draftF; rec[6] = r.draftA;
      batch.curveCount[inst]++;
      batch.nextCurve[inst] += batch.curveEvery;
    }
    device_step<Mixed>(C);
    ++done;
    if (traced && tid == 0) {
      if (done <= batch.denseSteps) {
        const int k = batch.denseCount[inst]++;
        write_record(batch.denseRec + (static_cast<std::size_t>(inst) * (batch.denseSteps + 1) + k) * batch.recSize, *C.st, *C.out, C.sc, NN);
      }
      if (C.st->t >= batch.nextSample[inst] - 1e-9 && batch.sampleCount[inst] < batch.maxSamples) {
        const int k = batch.sampleCount[inst]++;
        write_record(batch.sampleRec + (static_cast<std::size_t>(inst) * batch.maxSamples + k) * batch.recSize, *C.st, *C.out, C.sc, NN);
        batch.nextSample[inst] += batch.sampleEvery;
      }
    }
    __syncthreads();
  }

  // store the instance
  if (tid == 0) {
    batch.t[inst] = C.st->t; batch.zO[inst] = C.st->zO; batch.pitch[inst] = C.st->pitch; batch.roll[inst] = C.st->roll;
    batch.vz[inst] = C.st->vz; batch.wth[inst] = C.st->wth; batch.wph[inst] = C.st->wph;
    batch.inflow[inst] = C.out->inflow; batch.Vb[inst] = C.out->Vb; batch.hullTop[inst] = C.out->hullTop; batch.mw[inst] = C.out->mw;
    batch.bx[inst] = C.out->bx; batch.by[inst] = C.out->by; batch.bz[inst] = C.out->bz;
    batch.evCount[inst] = C.ev->count; batch.foundered[inst] = C.ev->foundered; batch.founderT[inst] = C.ev->founderT;
    batch.steps[inst] = done;
  }
  for (Index n = tid; n < NN; n += kBlockThreads) {
    batch.vol[instN + n] = vol[n]; batch.level[instN + n] = level[n];
    batch.cx[instN + n] = C.sc.cx[n]; batch.cy[instN + n] = C.sc.cy[n]; batch.cz[instN + n] = C.sc.cz[n];
  }
  for (Index e = tid; e < sim.evCap; e += kBlockThreads) batch.evItems[static_cast<std::size_t>(inst) * sim.evCap + e] = evItems[e];
  for (Index m = tid; m < sim.nMon; m += kBlockThreads) batch.overT[static_cast<std::size_t>(inst) * sim.nMon + m] = overT[m];
  for (Index m = tid; m < sim.nMarks; m += kBlockThreads) batch.markT[static_cast<std::size_t>(inst) * sim.nMarks + m] = markT[m];
}

}  // namespace sinksim::cuda
