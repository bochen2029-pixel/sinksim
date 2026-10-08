// One time step, in the oracle's order: pose, buoyancy, free-surface levels, flows, volumes, loads,
// rigid-body integration, events. Everything is a view; nothing allocates. Math chooses the transcendental
// functions; Order chooses how the column integrals are summed: the oracle's serial loops, the GPU's canonical
// tree in double precision, or the GPU's tree with single-precision partials (reduce.hpp).
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

enum class Order { Serial, Tree, TreeMixed };

template <Order O>
SS_HD inline Buoyancy hydro_pass_ordered(const ColumnsView& c, const Frame& F) {
  if constexpr (O == Order::Serial) return hydro_pass(c, F);
  else if constexpr (O == Order::Tree) return hydro_pass_tree<kBlockThreads>(c, F);
  else return hydro_pass_tree_mixed<kBlockThreads>(c, F);
}

template <Order O>
SS_HD inline void solve_levels_ordered(const ShipView& S, const GroupsView& G, const Frame& F, bool full, State& st, Scratch& sc) {
  if constexpr (O == Order::Serial) {
    const SerialPasser pass{S, F};
    solve_levels(S.nodes, G, F, pass, full, st, sc);
  } else if constexpr (O == Order::Tree) {
    const TreePasser<kWarpLanes> pass{S, F};
    solve_levels(S.nodes, G, F, pass, full, st, sc);
  } else {
    const TreePasserMixed<kWarpLanes> pass{S, F};
    solve_levels(S.nodes, G, F, pass, full, st, sc);
  }
}

template <class Math, Order O>
SS_HD inline void step(const ShipView& S, const SimView& M, State& st, Scratch& sc, Outputs& out, EventLog& ev) {
  const Frame F = pose_frame<Math>(M.body, st.zO, st.pitch, st.roll);
  const Buoyancy hb = hydro_pass_ordered<O>(S.cols, F);
  solve_levels_ordered<O>(S, M.groups, F, false, st, sc);
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
