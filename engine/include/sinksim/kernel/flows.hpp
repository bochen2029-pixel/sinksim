// Flows through the connections (orifice and weir laws with the stability limiter) and the volume update.
// The reference accumulates in connection order (scatter), exactly as the oracle does. A gather-ordered
// variant for the GPU (each node summing its own connections in a fixed order) is a separate policy and
// must be compared against this one at tolerance, not bit for bit.
#pragma once

#include "sinksim/kernel/math.hpp"
#include "sinksim/kernel/scheme.hpp"
#include "sinksim/kernel/types.hpp"

namespace sinksim::kernel {

// Effective heads and areas, connection degrees, then the flows. Returns the net volume taken from the sea.
SS_HD inline Real compute_flows(const ShipView& S, const SimView& M, const Frame& F, State& st, Scratch& sc) {
  const Index NN = S.nodes.n;
  for (Index n = 0; n < NN; ++n) {
    sc.hEff[n] = st.vol[n] > scheme::kEmptyVolume ? st.level[n] : scheme::kNoHead;  // an empty space exerts no head
    sc.aEff[n] = st.vol[n] >= S.nodes.vmax[n] ? S.nodes.aov[n] : ss_max(sc.area[n], S.nodes.amin[n]);
    sc.acc[n] = 0;
    sc.deg[n] = 0;
  }
  const ConnectionsView& C = M.conns;
  // how many openings act on each space this step; they share the stability limit
  for (Index c = 0; c < C.n; ++c) {
    if (!C.en[c]) continue;
    const Index a = C.a[c], b = C.b[c];
    if (a >= 0 && sc.hEff[a] < scheme::kNoHeadTest && sc.hEff[b] < scheme::kNoHeadTest) continue;
    if (a >= 0) sc.deg[a]++;
    sc.deg[b]++;
  }
  Real seaIn = 0;
  const Real dt = M.dt, sq2g = M.phys.sq2g;
  for (Index c = 0; c < C.n; ++c) {
    if (!C.en[c] || C.tOn[c] > st.t) { C.q[c] = 0; continue; }
    if (C.skipGroup[c] >= 0 && sc.merged[C.skipGroup[c]]) { C.q[c] = 0; continue; }
    const Index a = C.a[c], b = C.b[c];
    const Real zo = F.R20 * C.x[c] + F.R21 * C.y[c] + F.R22 * C.z[c] + F.tz;
    const Real ha = a < 0 ? Real(0) : sc.hEff[a], hbv = sc.hEff[b];
    const Real Ha = ha > zo ? ha - zo : Real(0), Hb = hbv > zo ? hbv - zo : Real(0);
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
          f = ss_pow(ss_max(Real(0), 1 - ss_pow(r, scheme::kWeirExponent)), scheme::kVillemonteExponent);
        }
        q = C.coef[c] * C.area[c] * scheme::kWeirCoefficient * sq2g * H1 * ss_sqrt(H1) * f * (Ha >= Hb ? Real(1) : Real(-1));
      }
    }
    if (q == 0) { C.q[c] = 0; continue; }
    Real dV = q * dt;
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
    C.q[c] = dV / dt;
    if (a >= 0) sc.acc[a] -= dV; else seaIn += dV;
    sc.acc[b] += dV;
  }
  return seaIn;
}

// Apply the accumulated changes. No space can be pressed above the sea surface: the excess returns to the sea.
SS_HD inline void update_volumes(const ShipView& S, State& st, Scratch& sc, Real& seaIn) {
  const Index NN = S.nodes.n;
  for (Index n = 0; n < NN; ++n) {
    Real v = st.vol[n] + sc.acc[n];
    if (v < 0) v = 0;
    const Real vm = S.nodes.vmax[n];
    if (v > vm) {
      const Real head = ss_max(Real(0), -sc.zmax[n]);
      const Real cap = vm + S.nodes.aov[n] * head;
      if (v > cap) { seaIn -= v - cap; v = cap; }
    }
    st.vol[n] = v;
  }
}

}  // namespace sinksim::kernel
