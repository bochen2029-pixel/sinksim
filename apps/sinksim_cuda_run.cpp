// sinksim_cuda_run: run a batch of identical simulations on the GPU, report throughput, optionally write the
// trace of the first instance (which the CPU in portable numerics must reproduce bit for bit: sinksim_check
// --numerics portable).
//   sinksim_cuda_run <ship.json> <sim.json> [--count N] [--tmax S] [--trace out.json] [--dense N] [--sample-every S]
//                    [--steps-per-launch K] [--perturb]
// --perturb gives every instance after the first a slightly different breach area, so that the batch is a sweep.
#include <algorithm>
#include <cstdio>
#include <exception>
#include <string>
#include <vector>

#include "cli.hpp"
#include "sinksim/cuda/batch.hpp"
#include "sinksim/io/compiled.hpp"
#include "sinksim/io/trace.hpp"

using namespace sinksim;

int main(int argc, char** argv) {
  const cli::Args args = cli::Args::parse(argc, argv, {"perturb"});
  if (args.positional.size() < 2) {
    std::fprintf(stderr, "usage: sinksim_cuda_run <ship.json> <sim.json> [--count N] [--tmax S] [--trace out.json] [--dense N] [--sample-every S] [--steps-per-launch K] [--perturb]\n");
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
    cuda::BatchOptions opt;
    opt.count = static_cast<Index>(args.num("count", 1));
    opt.tMax = args.num("tmax", 5 * 3600);
    opt.tracedInstances = args.has("trace") ? 1 : 0;
    opt.denseSteps = static_cast<int>(args.num("dense", 400));
    opt.sampleEvery = args.num("sample-every", 60);
    opt.stepsPerLaunch = static_cast<std::int64_t>(args.num("steps-per-launch", 2000));
    std::printf("device %s (sm_%d%d, %d SMs, %.1f GiB); runtime %d, driver %d\n", info.name.c_str(), info.major, info.minor, info.multiprocessors,
                info.globalMemory / 1073741824.0, info.runtimeVersion, info.driverVersion);
    std::printf("ship %s: %d columns, %d segments, %d nodes; sim %s: %d groups, %d connections; batch of %d\n", ship->id.c_str(), ship->columns(),
                ship->segments(), ship->nodes(), sim.id.c_str(), sim.groups(), sim.connections(), opt.count);
    cuda::Batch batch(ship, sim, opt);
    std::printf("shared memory per block %zu bytes, %lld steps per launch\n", batch.shared_memory_bytes(), static_cast<long long>(opt.stepsPerLaunch));
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
