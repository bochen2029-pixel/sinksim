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

}  // namespace sinksim
