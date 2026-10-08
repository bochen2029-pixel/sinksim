// sinksim_run: run one compiled simulation and report what happened; optionally write a trace.
//   sinksim_run <ship.json> <sim.json> [--numerics oracle|portable|std] [--tmax S] [--every S] [--stop-when-stable]
//                                      [--trace out.json] [--dense N] [--sample-every S] [--history]
#include <chrono>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>

#include "cli.hpp"
#include "sinksim/io/compiled.hpp"
#include "sinksim/io/trace.hpp"
#include "sinksim/simulation.hpp"

using namespace sinksim;

int main(int argc, char** argv) {
  const cli::Args args = cli::Args::parse(argc, argv, {"stop-when-stable", "history"});
  if (args.positional.size() < 2) {
    std::fprintf(stderr, "usage: sinksim_run <ship.json> <sim.json> [--numerics oracle|portable|std] [--tmax S] [--every S] [--stop-when-stable] [--trace out.json] [--dense N] [--sample-every S] [--history]\n");
    return 2;
  }
  const Numerics numerics = cli::numerics_from(args);
  try {
    const auto ship = io::load_ship(args.positional[0]);
    const CompiledSim sim = io::load_sim(args.positional[1]);
    io::check_pairing(*ship, sim);
    std::printf("ship %s: %d columns, %d segments, %d nodes\n", ship->id.c_str(), ship->columns(), ship->segments(), ship->nodes());
    std::printf("sim  %s: %s; %d groups, %d connections, dt %g s; numerics %s\n", sim.id.c_str(), sim.title.c_str(), sim.groups(), sim.connections(), sim.dt, numerics_name(numerics));
    Simulation S(ship, sim, numerics);

    const auto t0 = std::chrono::steady_clock::now();
    if (args.has("trace")) {
      io::Trace tr = S.record_trace(args.num("tmax", 5 * 3600), static_cast<int>(args.num("dense", 400)), args.num("sample-every", 60));
      tr.engine = "sinksim " + std::to_string(kEngineVersion.major) + "." + std::to_string(kEngineVersion.minor) + "." + std::to_string(kEngineVersion.patch);
      const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
      io::save_trace(tr, args.get("trace", "trace.json"));
      std::printf("%lld steps in %.0f ms (%.0f steps/s); %s; wrote %s with %d dense and %d sampled records\n", static_cast<long long>(tr.steps), ms,
                  tr.steps / (ms / 1000.0), tr.foundered ? ("foundered at " + cli::minutes(tr.founderT)).c_str() : "afloat", args.get("trace", "trace.json").c_str(),
                  tr.dense.n, tr.samples.n);
      for (const io::TraceEvent& e : tr.events) std::printf("  %9.2f min  %s\n", e.t / 60, e.label.c_str());
      return 0;
    }
    RunOptions opt;
    opt.tMax = args.num("tmax", 6 * 3600);
    opt.every = args.num("every", 30);
    opt.stopWhenStable = args.flag("stop-when-stable");
    const RunResult r = S.run(opt);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::printf("%lld steps in %.0f ms (%.0f steps/s)\n", static_cast<long long>(r.steps), ms, r.steps / (ms / 1000.0));
    if (r.foundered) std::printf("FOUNDERS at %s (%s)\n", cli::hm(r.founderT).c_str(), cli::minutes(r.founderT).c_str());
    else std::printf("afloat (checked to %s)\n", cli::hm(r.tEnd).c_str());
    std::printf("final: trim %.3f deg  list %.3f deg  draft fwd/aft %.2f / %.2f m  water %.0f t\n", r.final.trimDeg, r.final.listDeg, r.final.draftF, r.final.draftA, r.final.waterT);
    for (const Event& e : r.events) std::printf("  %9.2f min  %s\n", e.t / 60, S.event_label(e).c_str());
    if (args.flag("history")) {
      std::printf("   t_min    trim    list    water  inflow_tpm     dF     dA\n");
      for (const HistoryRecord& h : r.hist) std::printf("%8.2f %7.3f %7.3f %8.0f %11.1f %6.2f %6.2f\n", h.t / 60, h.trim, h.list, h.water, h.inflow, h.dF, h.dA);
    }
    return 0;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "sinksim_run: %s\n", e.what());
    return 1;
  }
}
