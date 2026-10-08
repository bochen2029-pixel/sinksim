// sinksim_sweep: run a sweep file on the CPU, many instances at a time, and write the results; or compare two
// results files exactly.
//   sinksim_sweep <ship.json> <sim.json> <sweep.json> [--numerics X] [--threads N] [--results out.json]
//   sinksim_sweep --compare a.results.json b.results.json
#include <chrono>
#include <cstdio>
#include <exception>
#include <string>

#include "cli.hpp"
#include "sinksim/io/compiled.hpp"
#include "sinksim/sweep.hpp"

using namespace sinksim;

int main(int argc, char** argv) {
  const cli::Args args = cli::Args::parse(argc, argv, {});
  try {
    if (args.has("compare")) {
      if (args.positional.size() < 1) {
        std::fprintf(stderr, "usage: sinksim_sweep --compare a.results.json b.results.json\n");
        return 2;
      }
      const sweep::Results a = sweep::load_results(args.get("compare", ""));
      const sweep::Results b = sweep::load_results(args.positional[0]);
      const sweep::Diff d = sweep::compare(a, b);
      std::printf("compared %zu values over %zu instances: %zu differ", d.compared, a.instances.size(), d.differing);
      if (d.differing) std::printf("; largest |diff| %.3e; first: %s", d.maxAbs, d.first.c_str());
      std::printf("\nRESULT: %s\n", d.differing == 0 ? "IDENTICAL" : "DIFFERENT");
      return d.differing == 0 ? 0 : 1;
    }
    if (args.positional.size() < 3) {
      std::fprintf(stderr, "usage: sinksim_sweep <ship.json> <sim.json> <sweep.json> [--numerics oracle|portable|portable32|std] [--threads N] [--results out.json]\n       sinksim_sweep --compare a.results.json b.results.json\n");
      return 2;
    }
    const Numerics numerics = cli::numerics_from(args);
    const auto ship = io::load_ship(args.positional[0]);
    const CompiledSim sim = io::load_sim(args.positional[1]);
    io::check_pairing(*ship, sim);
    const sweep::Sweep sw = sweep::load_sweep(args.positional[2], sim);
    const int threads = static_cast<int>(args.num("threads", 0));
    std::printf("sweep of %zu instances on %s / %s, numerics %s, tMax %s, curve every %g s, %d threads\n", sw.instances.size(), ship->id.c_str(), sim.id.c_str(),
                numerics_name(numerics), cli::minutes(sw.tMax).c_str(), sw.every, threads);
    const auto t0 = std::chrono::steady_clock::now();
    const sweep::Results r = sweep::run_cpu(ship, sim, sw, numerics, threads);
    const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::int64_t steps = 0;
    int foundered = 0;
    for (const sweep::InstanceResult& in : r.instances) { steps += in.steps; if (in.foundered) ++foundered; }
    std::printf("%lld steps in %.2f s (%.0f steps/s aggregate); %d of %zu instances foundered\n", static_cast<long long>(steps), s, steps / s, foundered, r.instances.size());
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
  } catch (const std::exception& e) {
    std::fprintf(stderr, "sinksim_sweep: %s\n", e.what());
    return 1;
  }
}
