// One time step, in the oracle's order: pose, buoyancy, free-surface levels, flows, volumes, loads,
// rigid-body integration, events. Everything is a view; nothing allocates. Math chooses the transcendental
// functions; TreeOrder chooses the GPU's reduction order (reduce.hpp) over the oracle's serial order.
#pragma once

#include "sinksim/kernel/body.hpp"
#include "sinksim/kernel/events.hpp"
#include "sinksim/kernel/flows.hpp"
#include "sinksim/kernel/frame.hpp"
#include "sinksim/kernel/hydro.hpp"
#include "sinksim/kernel/levels.hpp"
#include "sinksim/kernel/reduce.hpp"
#include "sinksim/kernel/types.hpp"

namespace sinksim::kernel {

template <class Math, bool TreeOrder>
SS_HD inline void step(const ShipView& S, const SimView& M, State& st, Scratch& sc, Outputs& out, EventLog& ev) {
  const Frame F = pose_frame<Math>(M.body, st.zO, st.pitch, st.roll);
  Buoyancy hb;
  if constexpr (TreeOrder) {
    hb = hydro_pass_tree<kBlockThreads>(S.cols, F);
    const TreePasser<kWarpLanes> pass{S, F};
    solve_levels(S.nodes, M.groups, F, pass, false, st, sc);
  } else {
    hb = hydro_pass(S.cols, F);
    const SerialPasser pass{S, F};
    solve_levels(S.nodes, M.groups, F, pass, false, st, sc);
  }
  Real seaIn = compute_flows<Math>(S, M, F, st, sc);
  update_volumes(S, st, sc, seaIn);
  out.inflow = seaIn / M.dt;
  const Loads L = loads(S, M, F, hb, st, sc);
  integrate(M, L, st);
  out.Vb = hb.V; out.hullTop = hb.top; out.mw = L.mw;
  out.bx = hb.x; out.by = hb.y; out.bz = hb.z;
  track_events(M, F, st, out, sc, ev);
}

}  // namespace sinksim::kernel
