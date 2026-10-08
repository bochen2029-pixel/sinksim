// Events: first overflow through each monitored set of connections, first submergence of each mark, and
// the founder rule. Events never feed back into the physics; they are bookkeeping on top of the state.
#pragma once

#include "sinksim/kernel/frame.hpp"
#include "sinksim/kernel/math.hpp"
#include "sinksim/kernel/types.hpp"

namespace sinksim::kernel {

SS_HD inline void track_events(const SimView& M, const Frame& F, const State& st, const Outputs& out, Scratch& sc,
                               EventLog& ev) {
  const ConnectionsView& C = M.conns;
  for (Index m = 0; m < M.monitors.n; ++m) sc.over[m] = 0;
  for (Index c = 0; c < C.n; ++c)
    if (C.monitor[c] >= 0 && C.q[c] > 0) sc.over[C.monitor[c]] += C.q[c];
  for (Index m = 0; m < M.monitors.n; ++m) {
    if (sc.over[m] > M.monitors.threshold && ev.overT[m] < 0) {
      ev.overT[m] = st.t;
      push_event(ev, st.t, m, EventKind::Overflow);
    }
  }
  for (Index i = 0; i < M.marks.n; ++i) {
    if (ev.markT[i] >= 0) continue;
    const Real wz = world_z(F, M.marks.x[i], M.marks.y[i], M.marks.z[i]);
    const bool under = M.marks.when[i] == static_cast<Byte>(MarkWhen::Under);
    if ((under && wz < 0) || (!under && wz > 0)) {
      ev.markT[i] = st.t;
      push_event(ev, st.t, i, EventKind::Mark);
    }
  }
  if (!ev.foundered && (out.hullTop < M.founder.hullTopBelow || ss_abs(st.pitch) > M.founder.pitchAbsAbove ||
                        (out.Vb < M.founder.buoyancyBelow && st.t > M.founder.minT))) {
    ev.foundered = 1;
    ev.founderT = st.t;
    push_event(ev, st.t, -1, EventKind::Founder);
  }
}

}  // namespace sinksim::kernel
