// Traces (docs/FORMATS.md, sinksim.trace): the complete dynamic state at every record, dense for the first
// steps and sampled afterwards, plus the events. A golden trace is a trace whose role is "golden".
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "sinksim/config.hpp"
#include "sinksim/kernel/types.hpp"
#include "sinksim/model.hpp"

namespace sinksim::io {

struct TraceColumns {
  Index n = 0;
  std::vector<Real> t, zO, pitch, roll, vz, wth, wph, inflow, Vb, hullTop;
  std::vector<Real> vol, level, cx, cy, cz;  // flat, n * nodes

  void push(const State& st, const Outputs& out, const Scratch& sc, Index nodes);
  StateSnapshot snapshot(Index i, Index nodes) const;
};

struct TraceEvent {
  Real t;
  std::string id, label;
};

struct Trace {
  std::string role = "trace", ship, sim, title, producer, engine;
  Real dt = 0;
  Index nodes = 0;
  int denseSteps = 0;
  Real sampleEvery = 0;
  std::int64_t steps = 0;
  Real tEnd = 0;
  bool foundered = false;
  Real founderT = -1;
  std::vector<TraceEvent> events;
  TraceColumns dense, samples;
};

Trace load_trace(const std::string& path);
void save_trace(const Trace& tr, const std::string& path);

}  // namespace sinksim::io
