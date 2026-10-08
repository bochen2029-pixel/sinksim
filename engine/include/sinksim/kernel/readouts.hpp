// Human-facing quantities derived from the state: trim and list in degrees, drafts at the perpendiculars,
// flood water in cubic metres and tonnes, inflow in tonnes per minute.
#pragma once

#include "sinksim/kernel/frame.hpp"
#include "sinksim/kernel/scheme.hpp"
#include "sinksim/kernel/types.hpp"

namespace sinksim::kernel {

struct Readouts {
  Real t;
  Real trimDeg;    // bow down positive
  Real listDeg;    // starboard down positive
  Real draftF, draftA;
  Real waterM3, waterT;
  Real inflowTpm;
};

template <class Math>
SS_HD inline Readouts readouts(const ShipView& S, const SimView& M, const ReadoutGeometry& R, const State& st,
                               const Outputs& out) {
  const Frame F = pose_frame<Math>(M.body, st.zO, st.pitch, st.roll);
  Readouts r;
  r.t = st.t;
  r.trimDeg = st.pitch * 180 / scheme::kPi;
  r.listDeg = st.roll * 180 / scheme::kPi;
  r.draftF = -world_z(F, R.xFP, 0, 0);
  r.draftA = -world_z(F, R.xAP, 0, 0);
  Real water = 0;
  for (Index n = 0; n < S.nodes.n; ++n) water += st.vol[n];
  r.waterM3 = water;
  r.waterT = water * M.phys.rho / 1000;
  r.inflowTpm = out.inflow * M.phys.rho / 1000 * 60;
  return r;
}

}  // namespace sinksim::kernel
