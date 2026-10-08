// Column integrals: buoyancy of the hull envelope below the sea surface, and the water in one node below a
// given world level. Two orders of summation exist for each: the serial order of the oracle, and the tree order
// of the GPU (reduce.hpp), which the CPU can emulate exactly. The level solvers take the node pass as a callable
// so that either order, or a block-cooperative GPU implementation, plugs in without touching the solver.
#pragma once

#include "sinksim/kernel/math.hpp"
#include "sinksim/kernel/reduce.hpp"
#include "sinksim/kernel/types.hpp"

namespace sinksim::kernel {

// Buoyancy of the hull envelope below the world plane Z = 0, with moments about the ship-frame origin (serial order).
SS_HD inline Buoyancy hydro_pass(const ColumnsView& c, const Frame& F) {
  const Real iR = 1 / F.R22;
  Real V = 0, Mx = 0, My = 0, Mz = 0, top = -1e9;
  for (Index i = 0; i < c.n; ++i) {
    const Real x = c.x[i], y = c.y[i], lo = c.zlo[i], hi = c.zhi[i];
    const Real base = F.R20 * x + F.R21 * y + F.tz;
    const Real wt = base + F.R22 * hi;
    if (wt > top) top = wt;
    const Real zp = -base * iR;
    if (zp <= lo) continue;
    const Real s = zp >= hi ? hi - lo : zp - lo;
    const Real v = c.dxdy[i] * s;
    V += v; Mx += v * x; My += v * y; Mz += v * (lo + 0.5 * s);
  }
  Buoyancy out;
  out.V = V;
  out.x = V > 0 ? Mx / V : 0;
  out.y = V > 0 ? My / V : 0;
  out.z = V > 0 ? Mz / V : 0;
  out.top = top;
  return out;
}

SS_HD inline Buoyancy finish_hydro(const HydroPartial& p) {
  Buoyancy out;
  out.V = p.V;
  out.x = p.V > 0 ? p.Mx / p.V : 0;
  out.y = p.V > 0 ? p.My / p.V : 0;
  out.z = p.V > 0 ? p.Mz / p.V : 0;
  out.top = p.top;
  return out;
}

// The same integral in the GPU's order: T thread partials, warp trees, then the warp results combined in order.
template <int T>
SS_HD inline Buoyancy hydro_pass_tree(const ColumnsView& c, const Frame& F) {
  constexpr int W = T / kWarpLanes;
  const Real iR = 1 / F.R22;
  HydroPartial warps[W];
  for (int w = 0; w < W; ++w) {
    HydroPartial lanes[kWarpLanes];
    for (int l = 0; l < kWarpLanes; ++l) {
      lanes[l] = hydro_zero();
      for (Index i = static_cast<Index>(w * kWarpLanes + l); i < c.n; i += T) hydro_column(c, F, iR, i, lanes[l]);
    }
    warps[w] = tree(lanes);
  }
  HydroPartial total = warps[0];
  for (int w = 1; w < W; ++w) combine(total, warps[w]);
  return finish_hydro(total);
}

// Water in node n below world level h, with the free-surface area and the moments of the water (serial order).
SS_HD inline NodePass node_pass(const ShipView& S, const Frame& F, Index n, Real h) {
  const ColumnsView& c = S.cols;
  const Index s0 = S.nodes.segStart[n], s1 = s0 + S.nodes.segCount[n];
  const Real iR = 1 / F.R22;
  Real vol = 0, dv = 0, mx = 0, my = 0, mz = 0, zmin = 1e9, zmax = -1e9;
  for (Index j = s0; j < s1; ++j) {
    const Index i = S.segs.col[j];
    const Real x = c.x[i], y = c.y[i], a = S.segs.a[j], b = S.segs.e[j];
    const Real base = F.R20 * x + F.R21 * y + F.tz;
    const Real wa = base + F.R22 * a, wb = base + F.R22 * b;
    if (wa < zmin) zmin = wa;
    if (wb > zmax) zmax = wb;
    const Real zp = (h - base) * iR;
    if (zp <= a) continue;
    const Real A = c.dxdy[i];
    Real L;
    if (zp >= b) {
      L = b - a;
    } else {
      L = zp - a;
      dv += A;
    }
    const Real v = A * L;
    vol += v; mx += v * x; my += v * y; mz += v * (a + 0.5 * L);
  }
  const Real mu = S.nodes.mu[n];
  NodePass r;
  r.vol = vol * mu;
  r.dv = dv * mu * iR;
  r.mx = mx * mu; r.my = my * mu; r.mz = mz * mu;
  r.zmin = zmin; r.zmax = zmax;
  return r;
}

SS_HD inline NodePass finish_pass(const ShipView& S, Index n, Real iR, const PassPartial& p) {
  const Real mu = S.nodes.mu[n];
  NodePass r;
  r.vol = p.vol * mu;
  r.dv = p.dv * mu * iR;
  r.mx = p.mx * mu; r.my = p.my * mu; r.mz = p.mz * mu;
  r.zmin = p.zmin; r.zmax = p.zmax;
  return r;
}

// The same integral in the GPU's warp order: L lane partials and the tree.
template <int L>
SS_HD inline NodePass node_pass_tree(const ShipView& S, const Frame& F, Index n, Real h) {
  const Index s0 = S.nodes.segStart[n], s1 = s0 + S.nodes.segCount[n];
  const Real iR = 1 / F.R22;
  PassPartial lanes[L];
  for (int l = 0; l < L; ++l) {
    lanes[l] = pass_zero();
    for (Index j = s0 + static_cast<Index>(l); j < s1; j += L) pass_segment(S, F, iR, h, j, lanes[l]);
  }
  return finish_pass(S, n, iR, tree(lanes));
}

// The node pass as a callable, the form the level solvers consume.
struct SerialPasser {
  const ShipView& S;
  const Frame& F;
  SS_HD NodePass operator()(Index n, Real h) const { return node_pass(S, F, n, h); }
};

template <int L>
struct TreePasser {
  const ShipView& S;
  const Frame& F;
  SS_HD NodePass operator()(Index n, Real h) const { return node_pass_tree<L>(S, F, n, h); }
};

}  // namespace sinksim::kernel
