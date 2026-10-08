// Partial sums and the canonical reduction tree shared by the GPU and its CPU emulation. A warp-level reduction
// has kWarpLanes partials, a block-level one kBlockThreads; each partial is a sequential sum over the elements
// with its stride, and the tree combines partial l with partial l + offset for offsets L/2, L/4, ..., 1, leaving
// the result in partial 0. The GPU performs exactly this with shuffles; the CPU runs the same loops, so the two
// produce the same bits.
#pragma once

#include "sinksim/kernel/types.hpp"

namespace sinksim::kernel {

inline constexpr int kWarpLanes = 32;
inline constexpr int kBlockThreads = 256;
inline constexpr int kBlockWarps = kBlockThreads / kWarpLanes;

struct HydroPartial {
  Real V, Mx, My, Mz, top;
};

struct PassPartial {
  Real vol, dv, mx, my, mz, zmin, zmax;
};

SS_HD inline HydroPartial hydro_zero() { return HydroPartial{0, 0, 0, 0, -1e9}; }
SS_HD inline PassPartial pass_zero() { return PassPartial{0, 0, 0, 0, 0, 1e9, -1e9}; }

SS_HD inline void combine(HydroPartial& a, const HydroPartial& b) {
  a.V += b.V; a.Mx += b.Mx; a.My += b.My; a.Mz += b.Mz;
  if (b.top > a.top) a.top = b.top;
}

SS_HD inline void combine(PassPartial& a, const PassPartial& b) {
  a.vol += b.vol; a.dv += b.dv; a.mx += b.mx; a.my += b.my; a.mz += b.mz;
  if (b.zmin < a.zmin) a.zmin = b.zmin;
  if (b.zmax > a.zmax) a.zmax = b.zmax;
}

// One column into a buoyancy partial: the same arithmetic as the serial hydro_pass.
SS_HD inline void hydro_column(const ColumnsView& c, const Frame& F, Real iR, Index i, HydroPartial& p) {
  const Real x = c.x[i], y = c.y[i], lo = c.zlo[i], hi = c.zhi[i];
  const Real base = F.R20 * x + F.R21 * y + F.tz;
  const Real wt = base + F.R22 * hi;
  if (wt > p.top) p.top = wt;
  const Real zp = -base * iR;
  if (zp <= lo) return;
  const Real s = zp >= hi ? hi - lo : zp - lo;
  const Real v = c.dxdy[i] * s;
  p.V += v; p.Mx += v * x; p.My += v * y; p.Mz += v * (lo + 0.5 * s);
}

// One segment into a node-pass partial: the same arithmetic as the serial node_pass.
SS_HD inline void pass_segment(const ShipView& S, const Frame& F, Real iR, Real h, Index j, PassPartial& p) {
  const Index i = S.segs.col[j];
  const Real x = S.cols.x[i], y = S.cols.y[i], a = S.segs.a[j], b = S.segs.e[j];
  const Real base = F.R20 * x + F.R21 * y + F.tz;
  const Real wa = base + F.R22 * a, wb = base + F.R22 * b;
  if (wa < p.zmin) p.zmin = wa;
  if (wb > p.zmax) p.zmax = wb;
  const Real zp = (h - base) * iR;
  if (zp <= a) return;
  const Real A = S.cols.dxdy[i];
  Real L;
  if (zp >= b) {
    L = b - a;
  } else {
    L = zp - a;
    p.dv += A;
  }
  const Real v = A * L;
  p.vol += v; p.mx += v * x; p.my += v * y; p.mz += v * (a + 0.5 * L);
}

// The canonical tree over L partials; the result is left in v[0].
template <class P, int L>
SS_HD inline P tree(P (&v)[L]) {
  for (int off = L / 2; off > 0; off >>= 1)
    for (int l = 0; l < off; ++l) combine(v[l], v[l + off]);
  return v[0];
}

}  // namespace sinksim::kernel
