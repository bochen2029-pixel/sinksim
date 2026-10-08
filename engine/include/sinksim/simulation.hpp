// Host-side simulation: owns the state, the scratch and the event log for one compiled simulation and drives
// the kernel. This is the object the command-line tools, the tests and later the WebAssembly binding use.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "sinksim/io/trace.hpp"
#include "sinksim/kernel/readouts.hpp"
#include "sinksim/kernel/types.hpp"
#include "sinksim/model.hpp"

namespace sinksim {

// Which bits the simulation computes (docs/DETERMINISM.md):
//   Oracle    oracle math (fdlibm sin/cos, the platform CRT's pow as V8 calls it) and the oracle's serial sums;
//             reproduces the JavaScript model bit for bit on Windows.
//   Portable  the host-and-device fdlibm port and the GPU's reduction order; the same bits on every CPU and GPU.
//   Std       the platform <cmath> with serial sums.
enum class Numerics { Oracle, Portable, Std };

const char* numerics_name(Numerics n);
bool parse_numerics(const std::string& s, Numerics& out);

struct RunOptions {
  Real tMax = 6 * 3600;        // s
  Real every = 30;             // s between history records
  bool stopWhenStable = false; // stop after 20 minutes of calm, once past 30 minutes (the oracle's rule)
};

struct HistoryRecord {
  Real t, trim, list, water, inflow, dF, dA;  // s, deg, deg, t, t/min, m, m
};

struct RunResult {
  bool foundered = false;
  Real founderT = -1;
  Real tEnd = 0;
  std::int64_t steps = 0;
  std::vector<HistoryRecord> hist;
  kernel::Readouts final{};
  std::vector<Event> events;
};

class Simulation {
 public:
  Simulation(std::shared_ptr<const CompiledShip> ship, const CompiledSim& sim, Numerics numerics = Numerics::Oracle);
  Simulation(const Simulation&) = delete;
  Simulation& operator=(const Simulation&) = delete;

  Numerics numerics() const { return numerics_; }

  void step();
  RunResult run(const RunOptions& opt, const std::function<bool(Simulation&)>& onStep = {});

  // A trace in the golden layout: the initial record, every step up to denseSteps, then every sampleEvery seconds.
  io::Trace record_trace(Real tMax, int denseSteps, Real sampleEvery);

  // State
  const State& state() const { return st_; }
  Index nodes() const { return nodes_; }
  // Exact restart from a snapshot (pose, rates, volumes, levels, centroids); event bookkeeping is cleared.
  void restore(const StateSnapshot& s);
  void reset_to_initial();
  void reset_events();
  // Evaluate buoyancy at the current pose without stepping (sets Vb and hullTop; inflow becomes 0).
  void evaluate_hydro();

  const Outputs& outputs() const { return out_; }
  kernel::Readouts readouts() const;
  const Scratch& scratch() const { return sc_; }
  const std::vector<Real>& flows() const { return q_; }

  bool foundered() const { return ev_.foundered != 0; }
  Real founderT() const { return ev_.founderT; }
  std::vector<Event> events() const;
  std::string event_id(const Event& e) const;
  std::string event_label(const Event& e) const;

  // Actions (shape-preserving: they only change connection fields)
  void set_connection_enabled(Index c, bool en);
  void set_connection_area(Index c, Real area);
  void set_connection_activation(Index c, Real tOn);
  Index set_kind_enabled(Byte kind, Index idx, bool en);  // every connection of that kind and idx; returns the count

  // Diagnostic: equivalent metacentric height from the roll stiffness with the water re-levelled.
  Real gm_now();

  const CompiledShip& ship() const { return *ship_; }
  const CompiledSim& sim() const { return sim_; }
  const Incidence& incidence() const { return inc_; }
  ShipView ship_view() const { return shipView_; }
  const SimView& sim_view() const { return simView_; }

 private:
  std::shared_ptr<const CompiledShip> ship_;
  CompiledSim sim_;
  Incidence inc_;
  Numerics numerics_;
  Index nodes_ = 0;
  ShipView shipView_{};
  SimView simView_{};

  std::vector<Real> vol_, level_;
  State st_{};

  std::vector<Real> area_, cx_, cy_, cz_, zmin_, zmax_, hEff_, aEff_, acc_, excess_, over_, q_, dV_;
  std::vector<Index> deg_;
  std::vector<Byte> merged_;
  std::vector<NodePass> passes_;
  Scratch sc_{};
  Outputs out_{};

  std::vector<Event> evItems_;
  std::vector<Real> overT_, markT_;
  EventLog ev_{};

  void bind();
  Buoyancy hydro_at(const Frame& F) const;
  Frame frame_now() const;
  void solve_levels_full(const Frame& F);
};

}  // namespace sinksim
