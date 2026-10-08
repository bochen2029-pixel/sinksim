// Rigid body: net vertical force and pitch/roll moments from buoyancy and the flood water, then
// semi-implicit Euler for heave, pitch and roll with linear and quadratic damping.
#pragma once

#include "sinksim/kernel/math.hpp"
#include "sinksim/kernel/scheme.hpp"
#include "sinksim/kernel/types.hpp"

namespace sinksim::kernel {

SS_HD inline Loads loads(const ShipView& S, const SimView& M, const Frame& F, const Buoyancy& hb, const State& st,
                         const Scratch& sc) {
  const Real W = M.phys.rho * M.phys.g;
  const Real Fb = W * hb.V;
  const Real bxw = F.R00 * hb.x + F.R01 * hb.y + F.R02 * hb.z + F.tx;
  const Real byw = F.R10 * hb.x + F.R11 * hb.y + F.R12 * hb.z + F.ty;
  Real tauX = (byw - M.body.YO) * Fb, tauY = -(bxw - M.body.XO) * Fb;
  Real Fz = Fb - M.phys.g * M.body.ms;
  Real mw = 0, Iw_p = 0, Iw_r = 0;
  const Index NN = S.nodes.n;
  for (Index n = 0; n < NN; ++n) {
    const Real V = st.vol[n];
    if (V <= 0) continue;
    const Real x = sc.cx[n], y = sc.cy[n], z = sc.cz[n];
    const Real wx = F.R00 * x + F.R01 * y + F.R02 * z + F.tx - M.body.XO;
    const Real wy = F.R10 * x + F.R11 * y + F.R12 * z + F.ty - M.body.YO;
    const Real wz = F.R20 * x + F.R21 * y + F.R22 * z + F.tz - st.zO;
    const Real f = -W * V;
    tauX += wy * f; tauY -= wx * f; Fz += f;
    const Real m = M.phys.rho * V;
    mw += m;
    Iw_p += m * (wx * wx + wz * wz);
    Iw_r += m * (wy * wy + wz * wz);
  }
  Loads L;
  L.Fz = Fz; L.Mth = tauY; L.Mph = tauX * F.cth;
  L.mw = mw; L.Iw_p = Iw_p; L.Iw_r = Iw_r;
  L.bxw = bxw; L.byw = byw;
  return L;
}

SS_HD inline void integrate(const SimView& M, const Loads& L, State& st) {
  const Real dt = M.dt;
  const Body& b = M.body;
  const Real mh = b.mh0 + L.mw;
  const Real Ith = b.Ith0 + L.Iw_p, Iph = b.Iph0 + L.Iw_r;
  const Real az = (L.Fz - b.cDz * st.vz - b.qz * ss_abs(st.vz) * st.vz) / mh;
  const Real ath = (L.Mth - b.cDth * st.wth - b.qth * ss_abs(st.wth) * st.wth) / Ith;
  const Real aph = (L.Mph - b.cDph * st.wph - b.qph * ss_abs(st.wph) * st.wph) / Iph;
  st.vz += az * dt; st.wth += ath * dt; st.wph += aph * dt;
  st.zO += st.vz * dt; st.pitch += st.wth * dt; st.roll += st.wph * dt;
  if (st.pitch > scheme::kPitchClamp) { st.pitch = scheme::kPitchClamp; st.wth = 0; }
  if (st.pitch < -scheme::kPitchClamp) { st.pitch = -scheme::kPitchClamp; st.wth = 0; }
  if (st.roll > scheme::kRollClamp) { st.roll = scheme::kRollClamp; st.wph = 0; }    // on her beam ends: stop there
  if (st.roll < -scheme::kRollClamp) { st.roll = -scheme::kRollClamp; st.wph = 0; }
  st.t += dt;
}

}  // namespace sinksim::kernel
