// Free-surface levels: for each node (or group of nodes sharing one surface) find the world level h at
// which the water below h equals the stored volume. Safeguarded Newton iteration bracketed by the node's
// world extent. The pass callable computes NodePass(n, h); see hydro.hpp.
#pragma once

#include "sinksim/kernel/frame.hpp"
#include "sinksim/kernel/math.hpp"
#include "sinksim/kernel/scheme.hpp"
#include "sinksim/kernel/types.hpp"

namespace sinksim::kernel {

template <class Passer>
SS_HD inline void solve_single(const NodesView& nodes, const Passer& pass, Index n, bool full, State& st, Scratch& sc) {
  const Real V = st.vol[n], Vmax = nodes.vmax[n];
  Real h = st.level[n];
  NodePass p = pass(n, h);
  const Real lo = p.zmin, hi = p.zmax;
  sc.zmin[n] = lo; sc.zmax[n] = hi;
  if (V <= scheme::kEmptyVolume) {
    st.level[n] = lo; sc.area[n] = 0;
    return;
  }
  if (V >= Vmax * scheme::kFullFraction) {
    p = pass(n, hi);
    st.level[n] = hi + ss_max(Real(0), V - Vmax) / nodes.aov[n];
    sc.area[n] = nodes.aov[n];
    if (p.vol > scheme::kCentroidMinVolume) { sc.cx[n] = p.mx / p.vol; sc.cy[n] = p.my / p.vol; sc.cz[n] = p.mz / p.vol; }
    return;
  }
  if (h < lo || h > hi) {
    h = ss_min(hi, ss_max(lo, h));
    p = pass(n, h);
  }
  const Real tol = full ? 1e-6 * Vmax + 1e-3 : ss_max(1e-3 * Vmax, Real(0.5));
  Real a = lo, b = hi;
  const int iters = full ? scheme::kNewtonFull : scheme::kNewtonFast;
  for (int it = 0; it < iters; ++it) {
    const Real err = p.vol - V;
    if (ss_abs(err) < tol) break;
    if (err > 0) b = h; else a = h;
    Real hn = p.dv > scheme::kSlopeMin ? h - err / p.dv : 0.5 * (a + b);
    if (!(hn > a && hn < b)) hn = 0.5 * (a + b);
    h = hn;
    p = pass(n, h);
  }
  st.level[n] = h; sc.area[n] = p.dv;
  if (p.vol > scheme::kCentroidMinVolume) { sc.cx[n] = p.mx / p.vol; sc.cy[n] = p.my / p.vol; sc.cz[n] = p.mz / p.vol; }
}

// N nodes sharing one free surface. The total volume is conserved exactly; the water is shared in
// proportion to the geometry at the solved level, capped by each member's capacity, the last member
// taking the remainder. For N = 2 this is the oracle's pair solve, operation for operation.
template <class Passer>
SS_HD inline void solve_group(const NodesView& nodes, const Passer& pass, const Index* members, Index N, bool full,
                              State& st, Scratch& sc) {
  NodePass* P = sc.passes;
  Real V = 0, Vmax = 0;
  for (Index k = 0; k < N; ++k) { V += st.vol[members[k]]; Vmax += nodes.vmax[members[k]]; }

  // initial guess: the mean level when every member has water, else the first wet member's level
  bool allWet = true;
  Index firstWet = -1;
  for (Index k = 0; k < N; ++k) {
    if (st.vol[members[k]] > 0) { if (firstWet < 0) firstWet = k; }
    else allWet = false;
  }
  Real h;
  if (allWet) {
    Real s = 0;
    for (Index k = 0; k < N; ++k) s += st.level[members[k]];
    h = s / Real(N);
  } else {
    h = st.level[members[firstWet >= 0 ? firstWet : N - 1]];
  }
  for (Index k = 0; k < N; ++k) P[k] = pass(members[k], h);
  for (Index k = 0; k < N; ++k) { sc.zmin[members[k]] = P[k].zmin; sc.zmax[members[k]] = P[k].zmax; }
  Real lo = P[0].zmin, hi = P[0].zmax;
  for (Index k = 1; k < N; ++k) { if (P[k].zmin < lo) lo = P[k].zmin; if (P[k].zmax > hi) hi = P[k].zmax; }

  if (V <= scheme::kEmptyVolume) {
    for (Index k = 0; k < N; ++k) { const Index n = members[k]; st.vol[n] = 0; st.level[n] = P[k].zmin; sc.area[n] = 0; }
    return;
  }
  if (V >= Vmax * scheme::kFullFraction) {
    const Real over = ss_max(Real(0), V - Vmax);
    Real aSum = 0;
    for (Index k = 0; k < N; ++k) aSum += nodes.aov[members[k]];
    const Real dh = over / aSum;
    Real assigned = 0;
    for (Index k = 0; k < N - 1; ++k) {
      const Real v = nodes.vmax[members[k]] + dh * nodes.aov[members[k]];
      st.vol[members[k]] = v;
      assigned += v;
    }
    st.vol[members[N - 1]] = V - assigned;
    for (Index k = 0; k < N; ++k) {
      const Index n = members[k];
      P[k] = pass(n, hi);
      st.level[n] = hi + dh; sc.area[n] = nodes.aov[n];
      if (P[k].vol > scheme::kCentroidMinVolume) { sc.cx[n] = P[k].mx / P[k].vol; sc.cy[n] = P[k].my / P[k].vol; sc.cz[n] = P[k].mz / P[k].vol; }
    }
    return;
  }
  if (!(h >= lo && h <= hi)) {
    h = ss_min(hi, ss_max(lo, h));
    for (Index k = 0; k < N; ++k) P[k] = pass(members[k], h);
  }
  const Real tol = full ? 1e-6 * Vmax + 1e-3 : ss_max(1e-3 * Vmax, Real(0.5));
  Real a = lo, b = hi;
  const int iters = full ? scheme::kNewtonFull : scheme::kNewtonFast;
  for (int it = 0; it < iters; ++it) {
    Real sum = 0;
    for (Index k = 0; k < N; ++k) sum += P[k].vol;
    const Real err = sum - V;
    if (ss_abs(err) < tol) break;
    if (err > 0) b = h; else a = h;
    Real dv = 0;
    for (Index k = 0; k < N; ++k) dv += P[k].dv;
    Real hn = dv > scheme::kSlopeMin ? h - err / dv : 0.5 * (a + b);
    if (!(hn > a && hn < b)) hn = 0.5 * (a + b);
    h = hn;
    for (Index k = 0; k < N; ++k) P[k] = pass(members[k], h);
  }
  // share the water in proportion to the geometry at this level; mass is conserved exactly
  Real tot = 0;
  for (Index k = 0; k < N; ++k) tot += P[k].vol;
  Real assigned = 0;
  for (Index k = 0; k < N - 1; ++k) {
    Real v = tot > 0 ? V * P[k].vol / tot : V / Real(N);
    if (v > nodes.vmax[members[k]]) v = nodes.vmax[members[k]];
    st.vol[members[k]] = v;
    assigned += v;
  }
  const Real vmaxLast = nodes.vmax[members[N - 1]];
  if (V - assigned > vmaxLast) {
    // the last member would overflow: the members before it hold V - vmaxLast between them, filling from the
    // one nearest the end and cascading forward if a member is itself full
    const Real need = V - vmaxLast;
    Real capped = 0;
    for (Index k = N - 2; k >= 0; --k) {
      Real prior = 0;
      for (Index j = 0; j < k; ++j) prior += st.vol[members[j]];
      const Real v = (need - capped) - prior;
      if (v > nodes.vmax[members[k]] && k > 0) {
        st.vol[members[k]] = nodes.vmax[members[k]];
        capped += nodes.vmax[members[k]];
        continue;
      }
      st.vol[members[k]] = v;
      break;
    }
    assigned = 0;
    for (Index k = 0; k < N - 1; ++k) assigned += st.vol[members[k]];
  }
  st.vol[members[N - 1]] = V - assigned;
  for (Index k = 0; k < N; ++k) {
    const Index n = members[k];
    st.level[n] = h; sc.area[n] = P[k].dv;
    if (P[k].vol > scheme::kCentroidMinVolume) { sc.cx[n] = P[k].mx / P[k].vol; sc.cy[n] = P[k].my / P[k].vol; sc.cz[n] = P[k].mz / P[k].vol; }
  }
}

// One group: decide whether its members share a surface this step, then solve. Groups are independent of each
// other, so a GPU block hands each one to a warp.
template <class Passer>
SS_HD inline void solve_group_entry(const NodesView& nodes, const GroupsView& G, const Frame& F, const Passer& pass, bool full,
                                    State& st, Scratch& sc, Index g) {
  const Index N = G.nodeCount[g];
  const Index* members = G.nodes + G.nodeStart[g];
  const GroupMode mode = static_cast<GroupMode>(G.mode[g]);
  bool merged = false;
  if (N >= 2) {
    if (mode == GroupMode::Always) {
      merged = true;
    } else if (mode == GroupMode::Threshold) {
      const Real zMw = world_z(F, G.px[g], G.py[g], G.pz[g]);
      merged = true;
      for (Index k = 0; k < N; ++k) {
        const Index n = members[k];
        if (!(st.vol[n] > scheme::kEmptyVolume && st.level[n] > zMw + G.margin)) { merged = false; break; }
      }
    }
  }
  sc.merged[g] = merged ? 1 : 0;
  if (merged) solve_group(nodes, pass, members, N, full, st, sc);
  else for (Index k = 0; k < N; ++k) solve_single(nodes, pass, members[k], full, st, sc);
}

template <class Passer>
SS_HD inline void solve_levels(const NodesView& nodes, const GroupsView& G, const Frame& F, const Passer& pass, bool full,
                               State& st, Scratch& sc) {
  for (Index g = 0; g < G.n; ++g) solve_group_entry(nodes, G, F, pass, full, st, sc, g);
}

}  // namespace sinksim::kernel
