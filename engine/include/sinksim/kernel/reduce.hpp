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

// ----------------------------------------------------------------------------- mixed precision
// The same partials with the column arithmetic and the partial sums in single precision. Geometry is converted
// from double as it is read, the pose enters as single-precision coefficients, and only the finished totals of a
// reduction return to double. Every operation is the same on the CPU and the GPU, so the bits are too.

struct FrameF {
  float R20, R21, R22, tz, iR;
};

SS_HD inline FrameF frame_f(const Frame& F) {
  FrameF f;
  f.R20 = static_cast<float>(F.R20);
  f.R21 = static_cast<float>(F.R21);
  f.R22 = static_cast<float>(F.R22);
  f.tz = static_cast<float>(F.tz);
  f.iR = 1.0f / f.R22;
  return f;
}

struct HydroPartialF {
  float V, Mx, My, Mz, top;
};

struct PassPartialF {
  float vol, dv, mx, my, mz, zmin, zmax;
};

SS_HD inline HydroPartialF hydro_zero_f() { return HydroPartialF{0.0f, 0.0f, 0.0f, 0.0f, -1e9f}; }
SS_HD inline PassPartialF pass_zero_f() { return PassPartialF{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1e9f, -1e9f}; }

SS_HD inline void combine(HydroPartialF& a, const HydroPartialF& b) {
  a.V += b.V; a.Mx += b.Mx; a.My += b.My; a.Mz += b.Mz;
  if (b.top > a.top) a.top = b.top;
}

SS_HD inline void combine(PassPartialF& a, const PassPartialF& b) {
  a.vol += b.vol; a.dv += b.dv; a.mx += b.mx; a.my += b.my; a.mz += b.mz;
  if (b.zmin < a.zmin) a.zmin = b.zmin;
  if (b.zmax > a.zmax) a.zmax = b.zmax;
}

SS_HD inline HydroPartial widen(const HydroPartialF& p) {
  return HydroPartial{static_cast<Real>(p.V), static_cast<Real>(p.Mx), static_cast<Real>(p.My), static_cast<Real>(p.Mz), static_cast<Real>(p.top)};
}

SS_HD inline PassPartial widen(const PassPartialF& p) {
  return PassPartial{static_cast<Real>(p.vol), static_cast<Real>(p.dv), static_cast<Real>(p.mx), static_cast<Real>(p.my), static_cast<Real>(p.mz),
                     static_cast<Real>(p.zmin), static_cast<Real>(p.zmax)};
}

SS_HD inline void hydro_column_f(const ColumnsView& c, const FrameF& f, Index i, HydroPartialF& p) {
  const float x = static_cast<float>(c.x[i]), y = static_cast<float>(c.y[i]);
  const float lo = static_cast<float>(c.zlo[i]), hi = static_cast<float>(c.zhi[i]);
  const float base = f.R20 * x + f.R21 * y + f.tz;
  const float wt = base + f.R22 * hi;
  if (wt > p.top) p.top = wt;
  const float zp = -base * f.iR;
  if (zp <= lo) return;
  const float s = zp >= hi ? hi - lo : zp - lo;
  const float v = static_cast<float>(c.dxdy[i]) * s;
  p.V += v; p.Mx += v * x; p.My += v * y; p.Mz += v * (lo + 0.5f * s);
}

SS_HD inline void pass_segment_f(const ShipView& S, const FrameF& f, float h, Index j, PassPartialF& p) {
  const Index i = S.segs.col[j];
  const float x = static_cast<float>(S.cols.x[i]), y = static_cast<float>(S.cols.y[i]);
  const float a = static_cast<float>(S.segs.a[j]), b = static_cast<float>(S.segs.e[j]);
  const float base = f.R20 * x + f.R21 * y + f.tz;
  const float wa = base + f.R22 * a, wb = base + f.R22 * b;
  if (wa < p.zmin) p.zmin = wa;
  if (wb > p.zmax) p.zmax = wb;
  const float zp = (h - base) * f.iR;
  if (zp <= a) return;
  const float A = static_cast<float>(S.cols.dxdy[i]);
  float L;
  if (zp >= b) {
    L = b - a;
  } else {
    L = zp - a;
    p.dv += A;
  }
  const float v = A * L;
  p.vol += v; p.mx += v * x; p.my += v * y; p.mz += v * (a + 0.5f * L);
}

}  // namespace sinksim::kernel
