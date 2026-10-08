// Sweeps: many instances of one compiled simulation that differ by scale factors on connection kinds and by
// per-connection overrides (enabled, activation time, area), run on the CPU or the GPU, with a readout curve per
// instance. This is the engine side of calibration and of the door-schedule questions: a file in, a file out,
// so any driver (Python, a notebook, another agent) can use it without a binding. Formats: docs/FORMATS.md.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "sinksim/kernel/readouts.hpp"
#include "sinksim/model.hpp"
#include "sinksim/simulation.hpp"

namespace sinksim::sweep {

// One override of a connection field. Either a connection index, or every connection of a kind (and idx unless -1).
struct ConnectionOverride {
  Index conn = -1;
  Byte kind = 255;
  Index idx = -1;   // -1: every connection of the kind
  bool hasEnabled = false, enabled = false;
  bool hasTOn = false;
  Real tOn = 0;
  bool hasArea = false;
  Real area = 0;
};

struct Instance {
  std::string id;
  std::vector<std::pair<Byte, Real>> kindScale;   // multiply the compiled area of every connection of that kind
  std::vector<ConnectionOverride> overrides;
};

struct Sweep {
  std::string ship, sim;
  Real tMax = 5 * 3600;
  Real every = 30;
  bool stopWhenStable = false;   // CPU only
  std::vector<Instance> instances;
};

Sweep load_sweep(const std::string& path, const CompiledSim& sim);
void save_sweep(const Sweep& sw, const std::string& path, const CompiledSim& sim);

// Apply an instance to any target through three setters (connection index, value).
template <class SetEnabled, class SetArea, class SetTOn>
void apply(const Instance& in, const CompiledSim& base, SetEnabled setEnabled, SetArea setArea, SetTOn setTOn) {
  const Index nc = base.connections();
  for (const auto& [kind, factor] : in.kindScale)
    for (Index c = 0; c < nc; ++c)
      if (base.cKind[static_cast<std::size_t>(c)] == kind) setArea(c, base.cArea[static_cast<std::size_t>(c)] * factor);
  for (const ConnectionOverride& o : in.overrides) {
    for (Index c = 0; c < nc; ++c) {
      const auto i = static_cast<std::size_t>(c);
      const bool match = o.conn >= 0 ? c == o.conn : (base.cKind[i] == o.kind && (o.idx < 0 || base.cIdx[i] == o.idx));
      if (!match) continue;
      if (o.hasEnabled) setEnabled(c, o.enabled);
      if (o.hasArea) setArea(c, o.area);
      if (o.hasTOn) setTOn(c, o.tOn);
    }
  }
}

void apply(const Instance& in, const CompiledSim& base, Simulation& S);

struct CurveRecord {
  Real t, trim, list, water, inflow, dF, dA;
};

struct EventRecord {
  Real t;
  std::string id, label;
};

struct InstanceResult {
  std::string id;
  bool foundered = false;
  Real founderT = -1;
  Real tEnd = 0;
  std::int64_t steps = 0;
  kernel::Readouts final{};
  std::vector<EventRecord> events;
  std::vector<CurveRecord> curve;
};

struct Results {
  std::string ship, sim, numerics, engine;
  Real tMax = 0, every = 0;
  std::vector<InstanceResult> instances;
};

// Run every instance on the CPU, `threads` at a time (0 = hardware concurrency).
Results run_cpu(std::shared_ptr<const CompiledShip> ship, const CompiledSim& sim, const Sweep& sw, Numerics numerics, int threads);

void save_results(const Results& r, const std::string& path);
Results load_results(const std::string& path);

// Exact comparison of two results files: the number of differing values and the first difference.
struct Diff {
  std::size_t differing = 0;
  std::size_t compared = 0;
  double maxAbs = 0;
  std::string first;
};
Diff compare(const Results& a, const Results& b);

}  // namespace sinksim::sweep
