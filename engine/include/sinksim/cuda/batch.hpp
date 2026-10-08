// CUDA batch engine: many independent simulations of one compiled ship and scenario, one thread block each,
// computing the portable numerics (docs/DETERMINISM.md) so that the CPU in portable mode reproduces every bit.
// This header has no CUDA types; it is usable from ordinary C++ when the library is linked.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "sinksim/io/trace.hpp"
#include "sinksim/model.hpp"

namespace sinksim::cuda {

// Is a CUDA device present and usable? On failure `why` receives the reason.
bool available(std::string* why = nullptr);

struct DeviceInfo {
  std::string name;
  int major = 0, minor = 0, multiprocessors = 0;
  std::size_t globalMemory = 0, sharedPerBlockOptIn = 0;
  int runtimeVersion = 0, driverVersion = 0;
};
DeviceInfo device_info();

struct BatchOptions {
  Index count = 1;                     // simulations in the batch
  int tracedInstances = 1;             // leading instances that record traces (0 = none)
  int denseSteps = 400;                // dense records at the start of each trace
  Real sampleEvery = 60;               // s between sampled records
  Real tMax = 5 * 3600;                // s; every instance stops here if it has not foundered
  std::int64_t stepsPerLaunch = 2000;  // kernel launches are bounded so a display GPU's watchdog never fires
};

struct InstanceResult {
  bool foundered = false;
  Real founderT = -1;
  Real tEnd = 0;
  std::int64_t steps = 0;
  Real t = 0, zO = 0, pitch = 0, roll = 0, vz = 0, wth = 0, wph = 0;
  std::vector<Real> vol, level, cx, cy, cz;
  Outputs outputs{};
  std::vector<Event> events;
};

class Batch {
 public:
  Batch(std::shared_ptr<const CompiledShip> ship, const CompiledSim& sim, const BatchOptions& opt);
  ~Batch();
  Batch(const Batch&) = delete;
  Batch& operator=(const Batch&) = delete;

  // Per-instance setup, before run(). Every instance starts from the compiled initial state and connection table.
  void restore(Index instance, const StateSnapshot& s);
  void set_connection_enabled(Index instance, Index c, bool en);
  void set_connection_area(Index instance, Index c, Real area);
  void set_connection_activation(Index instance, Index c, Real tOn);

  // Run every instance to its founder or tMax. Returns the wall time in seconds.
  double run();

  const BatchOptions& options() const;
  std::int64_t total_steps() const;
  std::size_t shared_memory_bytes() const;
  InstanceResult result(Index instance) const;
  io::Trace trace(Index instance) const;   // instance < options().tracedInstances
  std::string event_id(const Event& e) const;
  std::string event_label(const Event& e) const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace sinksim::cuda
