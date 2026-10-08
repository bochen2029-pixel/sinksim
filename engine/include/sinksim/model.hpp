// Owning containers for a compiled ship (geometry, shared by every simulation of that ship) and a compiled
// simulation (network, constants, initial state). They mirror the on-disk formats in docs/FORMATS.md and
// expose plain views for the kernel.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "sinksim/config.hpp"
#include "sinksim/kernel/types.hpp"

namespace sinksim {

struct CompiledShip {
  std::string id, title, hashAll;
  std::string producer;

  // columns
  std::vector<Real> x, y, dxdy, zlo, zhi;
  std::vector<Index> zone, side;
  // nodes
  std::vector<std::string> nodeLabel;
  std::vector<Real> mu, vmax, amin, aov;
  std::vector<Index> segStart, segCount;
  // segments
  std::vector<Index> segCol;
  std::vector<Real> segA, segE;

  Index columns() const { return static_cast<Index>(x.size()); }
  Index nodes() const { return static_cast<Index>(mu.size()); }
  Index segments() const { return static_cast<Index>(segCol.size()); }

  ShipView view() const;
};

// The complete dynamic state at one instant, as pointers into per-node storage. The centroids are part of
// the state: the oracle keeps a node's last computed centroid while the node holds less water than the solver
// tolerance, and still uses it in the loads (docs/DETERMINISM.md).
struct StateSnapshot {
  Real t, zO, pitch, roll, vz, wth, wph;
  const Real* vol;
  const Real* level;
  const Real* cx;
  const Real* cy;
  const Real* cz;
};

struct CompiledSim {
  std::string id, title, shipId, shipHash, hashAll;
  std::string producer;

  Physics phys{};
  Real dt = 0;

  // free-surface groups
  std::vector<Index> gNodeStart, gNodeCount, gNodes;
  std::vector<Byte> gMode;
  std::vector<Real> gpx, gpy, gpz;
  Real gMargin = 0;

  // connections
  std::vector<Index> ca, cb;
  std::vector<Byte> cLaw;
  std::vector<Real> cx, cy, cz, cArea, cCoef;
  std::vector<Byte> cKind;
  std::vector<Index> cIdx;
  std::vector<Byte> cEn;
  std::vector<Real> cTOn;
  std::vector<Index> cSkipGroup, cMonitor;
  std::vector<std::string> kindNames;

  // events
  Real monitorThreshold = 0;
  std::vector<std::string> monitorId, monitorLabel;
  std::vector<std::string> markId, markLabel;
  std::vector<Real> mkx, mky, mkz;
  std::vector<Byte> mkWhen;
  FounderRule founder{};

  Body body{};
  ReadoutGeometry readout{};

  // initial state
  Real t0 = 0, zO0 = 0, pitch0 = 0, roll0 = 0, vz0 = 0, wth0 = 0, wph0 = 0;
  std::vector<Real> vol0, level0, cx0, cy0, cz0;

  Index groups() const { return static_cast<Index>(gNodeCount.size()); }
  Index connections() const { return static_cast<Index>(ca.size()); }
  Index monitors() const { return static_cast<Index>(monitorId.size()); }
  Index marks() const { return static_cast<Index>(mkx.size()); }
  Index nodes() const { return static_cast<Index>(vol0.size()); }
  Index maxGroupSize() const;
  Index kindIndex(const std::string& name) const;  // -1 when the kind is not defined
  StateSnapshot initial() const { return StateSnapshot{t0, zO0, pitch0, roll0, vz0, wth0, wph0, vol0.data(), level0.data(), cx0.data(), cy0.data(), cz0.data()}; }
};

// Connections incident to each node (CSR, increasing connection index) and the sea connections in order.
// Derived from a compiled simulation; see IncidenceView.
struct Incidence {
  std::vector<Index> start, count, conn;
  std::vector<Byte> side;
  std::vector<Index> sea;
  IncidenceView view() const {
    return IncidenceView{start.data(), count.data(), conn.data(), side.data(), static_cast<Index>(sea.size()), sea.data()};
  }
};

Incidence build_incidence(const CompiledSim& sim);

}  // namespace sinksim
