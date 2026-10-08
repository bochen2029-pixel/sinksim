// Flows through the connections (orifice and weir laws with the stability limiter) and the volume update.
// The work is split into per-node and per-connection pieces so that a GPU block can spread them over threads;
// the serial driver below calls the same pieces in order. Accumulation per node is a gather over the node's
// incident connections in increasing connection order, which is the order a scatter in connection order would
// have used, so the CPU and the GPU sum the same terms in the same order.
#pragma once

#include "sinksim/kernel/math.hpp"
#include "sinksim/kernel/scheme.hpp"
#include "sinksim/kernel/types.hpp"

namespace sinksim::kernel {

// Effective head and free-surface area of one node for the limiter.
SS_HD inline void effective_heads(const ShipView& S, const State& st, Scratch& sc, Index n) {
  sc.hEff[n] = st.vol[n] > scheme::kEmptyVolume ? st.level[n] : scheme::kNoHead;  // an empty space exerts no head
  sc.aEff[n] = st.vol[n] >= S.nodes.vmax[n] ? S.nodes.aov[n] : ss_max(sc.area[n], S.nodes.amin[n]);
}

// Does connection c count toward the degrees this step (it is enabled and not between two empty spaces)?
SS_HD inline bool connection_counts(const ConnectionsView& C, const Scratch& sc, Index c) {
  if (!C.en[c]) return false;
  const Index a = C.a[c], b = C.b[c];
  if (a >= 0 && sc.hEff[a] < scheme::kNoHeadTest && sc.hEff[b] < scheme::kNoHeadTest) return false;
  return true;
}

// Number of counting connections at node n; they share the stability limit.
SS_HD inline void node_degree(const SimView& M, Scratch& sc, Index n) {
  const Index s = M.inc.start[n], e = s + M.inc.count[n];
  Index d = 0;
  for (Index k = s; k < e; ++k)
    if (connection_counts(M.conns, sc, M.inc.conn[k])) ++d;
  sc.deg[n] = d;
}

// Volume moved through connection c this step (a -> b positive), after the limiter; also records q[c].
template <class Math>
SS_HD inline Real connection_flow(const SimView& M, const Frame& F, const State& st, const Scratch& sc, Index c) {
  const ConnectionsView& C = M.conns;
  if (!C.en[c] || C.tOn[c] > st.t) { C.q[c] = 0; return 0; }
  if (C.skipGroup[c] >= 0 && sc.merged[C.skipGroup[c]]) { C.q[c] = 0; return 0; }
  const Index a = C.a[c], b = C.b[c];
  const Real zo = F.R20 * C.x[c] + F.R21 * C.y[c] + F.R22 * C.z[c] + F.tz;
  const Real ha = a < 0 ? Real(0) : sc.hEff[a], hbv = sc.hEff[b];
  const Real Ha = ha > zo ? ha - zo : Real(0), Hb = hbv > zo ? hbv - zo : Real(0);
  const Real sq2g = M.phys.sq2g;
  Real q = 0;
  if (C.law[c] == static_cast<Byte>(FlowLaw::Orifice)) {
    const Real d = Ha - Hb;
    if (d != 0) q = C.coef[c] * C.area[c] * sq2g * d / ss_sqrt(ss_max(ss_abs(d), scheme::kOrificeHeadFloor));
  } else {
    const Real H1 = Ha > Hb ? Ha : Hb;
    if (H1 > 0) {
      const Real H2 = Ha > Hb ? Hb : Ha;
      Real f = 1;
      if (H2 > 0) {
        const Real r = H2 / H1;
        f = Math::pow(ss_max(Real(0), 1 - Math::pow(r, scheme::kWeirExponent)), scheme::kVillemonteExponent);
      }
      q = C.coef[c] * C.area[c] * scheme::kWeirCoefficient * sq2g * H1 * ss_sqrt(H1) * f * (Ha >= Hb ? Real(1) : Real(-1));
    }
  }
  if (q == 0) { C.q[c] = 0; return 0; }
  Real dV = q * M.dt;
  // stability limit: never move more than a share of what would level the two sides
  const Index src = dV > 0 ? a : b, dst = dV > 0 ? b : a;
  const Real hs = src < 0 ? Real(0) : sc.hEff[src];
  const Real hd = dst < 0 ? Real(0) : sc.hEff[dst];
  const Real inv = (src < 0 ? Real(0) : 1 / sc.aEff[src]) + (dst < 0 ? Real(0) : 1 / sc.aEff[dst]);
  const Index degRef = src < 0 ? sc.deg[dst] : (dst < 0 ? sc.deg[src] : (sc.deg[src] > sc.deg[dst] ? sc.deg[src] : sc.deg[dst]));
  const Real share = scheme::kLimiterShare / Real(degRef > 1 ? degRef : 1);
  Real lim = inv > 0 ? share * (hs - ss_max(hd, zo)) / inv : ss_abs(dV);
  if (lim < 0) lim = 0;
  if (ss_abs(dV) > lim) dV = dV > 0 ? lim : -lim;
  C.q[c] = dV / M.dt;
  return dV;
}

// Net volume change of node n: its incident connections in increasing order, +dV at the b end, -dV at the a end.
SS_HD inline void node_accumulate(const SimView& M, Scratch& sc, Index n) {
  const Index s = M.inc.start[n], e = s + M.inc.count[n];
  Real a = 0;
  for (Index k = s; k < e; ++k) {
    const Real d = sc.dV[M.inc.conn[k]];
    a = M.inc.side[k] ? a + d : a - d;
  }
  sc.acc[n] = a;
}

// Volume taken from the sea this step: the sea connections in increasing order.
SS_HD inline Real sea_inflow(const SimView& M, const Scratch& sc) {
  Real s = 0;
  for (Index k = 0; k < M.inc.seaCount; ++k) s += sc.dV[M.inc.sea[k]];
  return s;
}

// Apply the change to node n. No space can be pressed above the sea surface: the excess is recorded and returns to the sea.
SS_HD inline void node_volume_update(const ShipView& S, State& st, Scratch& sc, Index n) {
  Real v = st.vol[n] + sc.acc[n];
  if (v < 0) v = 0;
  const Real vm = S.nodes.vmax[n];
  Real excess = 0;
  if (v > vm) {
    const Real head = ss_max(Real(0), -sc.zmax[n]);
    const Real cap = vm + S.nodes.aov[n] * head;
    if (v > cap) { excess = v - cap; v = cap; }
  }
  sc.excess[n] = excess;
  st.vol[n] = v;
}

// The excess returned to the sea, subtracted from the inflow in node order.
SS_HD inline Real return_excess(const ShipView& S, const Scratch& sc, Real seaIn) {
  for (Index n = 0; n < S.nodes.n; ++n)
    if (sc.excess[n] > 0) seaIn -= sc.excess[n];
  return seaIn;
}

// Serial driver: the pieces above in order. Returns the net volume taken from the sea before the excess is returned.
template <class Math>
SS_HD inline Real compute_flows(const ShipView& S, const SimView& M, const Frame& F, State& st, Scratch& sc) {
  const Index NN = S.nodes.n;
  for (Index n = 0; n < NN; ++n) effective_heads(S, st, sc, n);
  for (Index n = 0; n < NN; ++n) node_degree(M, sc, n);
  for (Index c = 0; c < M.conns.n; ++c) sc.dV[c] = connection_flow<Math>(M, F, st, sc, c);
  for (Index n = 0; n < NN; ++n) node_accumulate(M, sc, n);
  return sea_inflow(M, sc);
}

SS_HD inline void update_volumes(const ShipView& S, State& st, Scratch& sc, Real& seaIn) {
  for (Index n = 0; n < S.nodes.n; ++n) node_volume_update(S, st, sc, n);
  seaIn = return_excess(S, sc, seaIn);
}

}  // namespace sinksim::kernel
