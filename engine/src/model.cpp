#include "sinksim/model.hpp"

namespace sinksim {

ShipView CompiledShip::view() const {
  ShipView v;
  v.cols = ColumnsView{columns(), x.data(), y.data(), dxdy.data(), zlo.data(), zhi.data()};
  v.segs = SegmentsView{segments(), segCol.data(), segA.data(), segE.data()};
  v.nodes = NodesView{nodes(), mu.data(), vmax.data(), amin.data(), aov.data(), segStart.data(), segCount.data()};
  return v;
}

Index CompiledSim::maxGroupSize() const {
  Index m = 1;
  for (const Index c : gNodeCount)
    if (c > m) m = c;
  return m;
}

Index CompiledSim::kindIndex(const std::string& name) const {
  for (std::size_t i = 0; i < kindNames.size(); ++i)
    if (kindNames[i] == name) return static_cast<Index>(i);
  return -1;
}

Incidence build_incidence(const CompiledSim& sim) {
  const auto nn = static_cast<std::size_t>(sim.nodes());
  const Index nc = sim.connections();
  Incidence inc;
  inc.count.assign(nn, 0);
  for (Index c = 0; c < nc; ++c) {
    const auto i = static_cast<std::size_t>(c);
    if (sim.ca[i] >= 0) inc.count[static_cast<std::size_t>(sim.ca[i])]++;
    inc.count[static_cast<std::size_t>(sim.cb[i])]++;
  }
  inc.start.assign(nn, 0);
  Index total = 0;
  for (std::size_t n = 0; n < nn; ++n) { inc.start[n] = total; total += inc.count[n]; }
  inc.conn.assign(static_cast<std::size_t>(total), -1);
  inc.side.assign(static_cast<std::size_t>(total), 0);
  std::vector<Index> fill(nn, 0);
  for (Index c = 0; c < nc; ++c) {
    const auto i = static_cast<std::size_t>(c);
    if (sim.ca[i] >= 0) {
      const auto n = static_cast<std::size_t>(sim.ca[i]);
      const auto k = static_cast<std::size_t>(inc.start[n] + fill[n]++);
      inc.conn[k] = c; inc.side[k] = 0;
    } else {
      inc.sea.push_back(c);
    }
    const auto n = static_cast<std::size_t>(sim.cb[i]);
    const auto k = static_cast<std::size_t>(inc.start[n] + fill[n]++);
    inc.conn[k] = c; inc.side[k] = 1;
  }
  return inc;
}

}  // namespace sinksim
