// sinksim_cuda_run: run a batch of simulations on the GPU, report throughput, optionally write the trace of the
// first instance (which the CPU in portable numerics must reproduce bit for bit: sinksim_check --numerics
// portable32 or portable), or run a sweep file and write its results.
//   sinksim_cuda_run <ship.json> <sim.json> [--count N] [--precision mixed|double] [--tmax S] [--trace out.json]
//                    [--dense N] [--sample-every S] [--steps-per-launch K] [--perturb]
//   sinksim_cuda_run <ship.json> <sim.json> --sweep sweep.json --results out.json [--precision mixed|double]
// --precision mixed (default) computes the portable32 numerics, double the portable numerics.
// --perturb gives every instance after the first a slightly different breach area, so that the batch is a sweep.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <exception>
#include <string>
#include <vector>

#include "cli.hpp"
#include "sinksim/cuda/batch.hpp"
#include "sinksim/io/compiled.hpp"
#include "sinksim/io/trace.hpp"
#include "sinksim/sweep.hpp"

using namespace sinksim;

namespace {

int run_sweep(const cli::Args& args, std::shared_ptr<const CompiledShip> ship, const CompiledSim& sim, bool mixed, const cuda::DeviceInfo& info) {
  const sweep::Sweep sw = sweep::load_sweep(args.get("sweep", ""), sim);
  if (sw.stopWhenStable) throw std::runtime_error("stopWhenStable is not supported on the GPU; run the sweep with sinksim_sweep");
  if (sw.instances.empty()) throw std::runtime_error("the sweep has no instances");
  cuda::BatchOptions opt;
  opt.count = static_cast<Index>(sw.instances.size());
  opt.mixedPrecision = mixed;
  opt.tracedInstances = 0;
  opt.tMax = sw.tMax;
  opt.curveEvery = sw.every;
  opt.stepsPerLaunch = static_cast<std::int64_t>(args.num("steps-per-launch", 2000));
  std::printf("sweep of %zu instances on %s / %s, numerics %s, tMax %s, curve every %g s\n", sw.instances.size(), ship->id.c_str(), sim.id.c_str(),
              mixed ? "portable32" : "portable", cli::minutes(sw.tMax).c_str(), sw.every);
  cuda::Batch batch(ship, sim, opt);
  for (Index i = 0; i < opt.count; ++i)
    sweep::apply(sw.instances[static_cast<std::size_t>(i)], sim,
                 [&](Index c, bool en) { batch.set_connection_enabled(i, c, en); },
                 [&](Index c, Real a) { batch.set_connection_area(i, c, a); },
                 [&](Index c, Real t) { batch.set_connection_activation(i, c, t); });
  const double seconds = batch.run();
  const std::int64_t steps = batch.total_steps();
  std::printf("%lld steps in %.2f s (%.0f steps/s aggregate)\n", static_cast<long long>(steps), seconds, steps / seconds);
  sweep::Results r;
  r.ship = ship->id;
  r.sim = sim.id;
  r.numerics = mixed ? "portable32" : "portable";
  r.engine = "sinksim CUDA engine on " + info.name;
  r.tMax = sw.tMax;
  r.every = sw.every;
  int foundered = 0;
  for (Index i = 0; i < opt.count; ++i) {
    const cuda::InstanceResult ir = batch.result(i);
    sweep::InstanceResult res;
    res.id = sw.instances[static_cast<std::size_t>(i)].id;
    res.foundered = ir.foundered;
    res.founderT = ir.founderT;
    res.tEnd = ir.tEnd;
    res.steps = ir.steps;
    const cuda::CurveSample fin = batch.final_readouts(i);
    res.final.t = fin.t; res.final.trimDeg = fin.trim; res.final.listDeg = fin.list; res.final.waterT = fin.water;
    res.final.inflowTpm = fin.inflow; res.final.draftF = fin.draftF; res.final.draftA = fin.draftA; res.final.waterM3 = fin.water / sim.phys.rho * 1000;
    for (const Event& e : ir.events) res.events.push_back(sweep::EventRecord{e.t, batch.event_id(e), batch.event_label(e)});
    for (const cuda::CurveSample& c : batch.curve(i)) res.curve.push_back(sweep::CurveRecord{c.t, c.trim, c.list, c.water, c.inflow, c.draftF, c.draftA});
    if (ir.foundered) ++foundered;
    r.instances.push_back(std::move(res));
  }
  std::printf("%d of %d instances foundered\n", foundered, opt.count);
  for (std::size_t i = 0; i < std::min<std::size_t>(r.instances.size(), 12); ++i) {
    const sweep::InstanceResult& in = r.instances[i];
    std::printf("  %-16s %s  trim %.2f  list %.2f  water %.0f t\n", in.id.c_str(), in.foundered ? ("founders at " + cli::minutes(in.founderT)).c_str() : "afloat", in.final.trimDeg, in.final.listDeg, in.final.waterT);
  }
  if (r.instances.size() > 12) std::printf("  ... %zu more\n", r.instances.size() - 12);
  if (args.has("results")) {
    sweep::save_results(r, args.get("results", ""));
    std::printf("wrote %s\n", args.get("results", "").c_str());
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  const cli::Args args = cli::Args::parse(argc, argv, {"perturb"});
  if (args.positional.size() < 2) {
    std::fprintf(stderr, "usage: sinksim_cuda_run <ship.json> <sim.json> [--count N] [--precision mixed|double] [--tmax S] [--trace out.json] [--dense N] [--sample-every S] [--steps-per-launch K] [--perturb]\n"
                         "       sinksim_cuda_run <ship.json> <sim.json> --sweep sweep.json --results out.json [--precision mixed|double]\n");
    return 2;
  }
  const std::string precision = args.get("precision", "mixed");
  if (precision != "mixed" && precision != "double") {
    std::fprintf(stderr, "unknown --precision %s (use mixed or double)\n", precision.c_str());
    return 2;
  }
  try {
    std::string why;
    if (!cuda::available(&why)) {
      std::fprintf(stderr, "sinksim_cuda_run: no usable CUDA device (%s)\n", why.c_str());
      return 1;
    }
    const cuda::DeviceInfo info = cuda::device_info();
    const auto ship = io::load_ship(args.positional[0]);
    const CompiledSim sim = io::load_sim(args.positional[1]);
    io::check_pairing(*ship, sim);
    std::printf("device %s (sm_%d%d, %d SMs, %.1f GiB); runtime %d, driver %d\n", info.name.c_str(), info.major, info.minor, info.multiprocessors,
                info.globalMemory / 1073741824.0, info.runtimeVersion, info.driverVersion);
    if (args.has("sweep")) return run_sweep(args, ship, sim, precision == "mixed", info);

    cuda::BatchOptions opt;
    opt.count = static_cast<Index>(args.num("count", 1));
    opt.mixedPrecision = precision == "mixed";
    opt.tMax = args.num("tmax", 5 * 3600);
    opt.tracedInstances = args.has("trace") ? 1 : 0;
    opt.denseSteps = static_cast<int>(args.num("dense", 400));
    opt.sampleEvery = args.num("sample-every", 60);
    opt.stepsPerLaunch = static_cast<std::int64_t>(args.num("steps-per-launch", 2000));
    std::printf("ship %s: %d columns, %d segments, %d nodes; sim %s: %d groups, %d connections; batch of %d\n", ship->id.c_str(), ship->columns(),
                ship->segments(), ship->nodes(), sim.id.c_str(), sim.groups(), sim.connections(), opt.count);
    cuda::Batch batch(ship, sim, opt);
    std::printf("numerics %s; shared memory per block %zu bytes, %lld steps per launch\n", opt.mixedPrecision ? "portable32 (mixed precision)" : "portable (double precision)",
                batch.shared_memory_bytes(), static_cast<long long>(opt.stepsPerLaunch));
    if (args.flag("perturb")) {
      // scale every breach of instance i by 1 + 0.02 * i / count: a sweep of the damage size
      const Index breachKind = sim.kindIndex("breach");
      for (Index i = 1; i < opt.count; ++i) {
        const Real scale = 1 + 0.02 * static_cast<Real>(i) / static_cast<Real>(opt.count);
        for (Index c = 0; c < sim.connections(); ++c)
          if (breachKind >= 0 && sim.cKind[static_cast<std::size_t>(c)] == static_cast<Byte>(breachKind))
            batch.set_connection_area(i, c, sim.cArea[static_cast<std::size_t>(c)] * scale);
      }
    }
    const double seconds = batch.run();
    const std::int64_t steps = batch.total_steps();
    std::printf("%lld steps over %d instances in %.3f s: %.0f steps/s aggregate, %.1f simulated hours per wall second, %.0f full sinkings per minute at this length\n",
                static_cast<long long>(steps), opt.count, seconds, steps / seconds, steps * sim.dt / 3600.0 / seconds,
                (steps / seconds) / (batch.result(0).steps > 0 ? static_cast<double>(batch.result(0).steps) : 1.0) * 60.0);
    Real tMin = 1e9, tMax = -1;
    int foundered = 0;
    for (Index i = 0; i < opt.count; ++i) {
      const cuda::InstanceResult r = batch.result(i);
      if (r.foundered) { ++foundered; tMin = std::min(tMin, r.founderT); tMax = std::max(tMax, r.founderT); }
    }
    if (foundered) std::printf("%d of %d instances foundered, between %s and %s\n", foundered, opt.count, cli::minutes(tMin).c_str(), cli::minutes(tMax).c_str());
    else std::printf("no instance foundered within %s\n", cli::minutes(opt.tMax).c_str());
    const cuda::InstanceResult r0 = batch.result(0);
    for (const Event& e : r0.events) std::printf("  %9.2f min  %s\n", e.t / 60, batch.event_label(e).c_str());
    if (args.has("trace")) {
      const io::Trace tr = batch.trace(0);
      io::save_trace(tr, args.get("trace", "cuda.trace.json"));
      std::printf("wrote %s with %d dense and %d sampled records\n", args.get("trace", "cuda.trace.json").c_str(), tr.dense.n, tr.samples.n);
    }
    return 0;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "sinksim_cuda_run: %s\n", e.what());
    return 1;
  }
}
