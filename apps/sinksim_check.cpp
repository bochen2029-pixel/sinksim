// sinksim_check: hold the engine to a reference trace, window by window and end to end.
//   sinksim_check <ship.json> <sim.json> <reference.trace.json>
//                 [--tol-step X] [--tol-window X] [--tol-founder S] [--tol-event S] [--quiet]
// 1. Dense records: restart from each record, take one step, compare with the next record.
// 2. Sampled records: restart from each sample, step to the next, compare.
// 3. End to end: run from the initial state and compare samples, events and the founder time.
// The window tests isolate the per-step error of the port from the amplification of the model
// (docs/DETERMINISM.md); the end-to-end test shows how that error grows.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

#include "cli.hpp"
#include "sinksim/io/compiled.hpp"
#include "sinksim/io/trace.hpp"
#include "sinksim/simulation.hpp"

using namespace sinksim;

namespace {

struct Diff {
  double worst = 0;
  std::string where;
  double pose = 0, rates = 0, vol = 0, level = 0, centroid = 0, outputs = 0;
  void note(const char* name, double a, double b, double& cat, int index = -1) {
    const double d = std::fabs(a - b);
    if (d > cat) cat = d;
    if (d > worst) { worst = d; where = index >= 0 ? std::string(name) + "[" + std::to_string(index) + "]" : name; }
  }
};

Diff compare(const Simulation& S, const io::TraceColumns& c, Index i) {
  Diff d;
  const State& st = S.state();
  const Outputs& o = S.outputs();
  const Scratch& sc = S.scratch();
  const Index N = S.nodes();
  const auto k = static_cast<std::size_t>(i);
  d.note("t", st.t, c.t[k], d.pose);
  d.note("zO", st.zO, c.zO[k], d.pose);
  d.note("pitch", st.pitch, c.pitch[k], d.pose);
  d.note("roll", st.roll, c.roll[k], d.pose);
  d.note("vz", st.vz, c.vz[k], d.rates);
  d.note("wth", st.wth, c.wth[k], d.rates);
  d.note("wph", st.wph, c.wph[k], d.rates);
  d.note("inflow", o.inflow, c.inflow[k], d.outputs);
  d.note("Vb", o.Vb, c.Vb[k], d.outputs);
  d.note("hullTop", o.hullTop, c.hullTop[k], d.outputs);
  for (Index n = 0; n < N; ++n) {
    const auto j = k * static_cast<std::size_t>(N) + static_cast<std::size_t>(n);
    d.note("vol", st.vol[n], c.vol[j], d.vol, n);
    d.note("level", st.level[n], c.level[j], d.level, n);
    d.note("cx", sc.cx[n], c.cx[j], d.centroid, n);
    d.note("cy", sc.cy[n], c.cy[j], d.centroid, n);
    d.note("cz", sc.cz[n], c.cz[j], d.centroid, n);
  }
  return d;
}

struct Worst {
  Diff d;
  std::string at;
  void merge(const Diff& x, const std::string& where) {
    if (x.worst > d.worst) { d.worst = x.worst; d.where = x.where; at = where; }
    d.pose = std::fmax(d.pose, x.pose); d.rates = std::fmax(d.rates, x.rates);
    d.vol = std::fmax(d.vol, x.vol); d.level = std::fmax(d.level, x.level); d.centroid = std::fmax(d.centroid, x.centroid); d.outputs = std::fmax(d.outputs, x.outputs);
  }
};

void print_worst(const char* title, const Worst& w, int count) {
  std::printf("%-20s %5d   max|diff| %.3e%s   pose %.1e  rates %.1e  vol %.1e m3  level %.1e m  centroid %.1e m  outputs %.1e\n", title, count,
              w.d.worst, w.d.worst > 0 ? (" (" + w.d.where + " at " + w.at + ")").c_str() : " (exact)", w.d.pose, w.d.rates, w.d.vol, w.d.level, w.d.centroid, w.d.outputs);
}

}  // namespace

int main(int argc, char** argv) {
  const cli::Args args = cli::Args::parse(argc, argv, {"quiet"});
  if (args.positional.size() < 3) {
    std::fprintf(stderr, "usage: sinksim_check <ship.json> <sim.json> <reference.trace.json> [--numerics oracle|portable|std] [--tol-step X] [--tol-window X] [--tol-founder S] [--tol-event S] [--quiet]\n");
    return 2;
  }
  const Numerics numerics = cli::numerics_from(args);
  const double tolStep = args.num("tol-step", 0), tolWindow = args.num("tol-window", 0);
  const double tolFounder = args.num("tol-founder", 1.0), tolEvent = args.num("tol-event", 30.0);
  const bool quiet = args.flag("quiet");
  try {
    const auto ship = io::load_ship(args.positional[0]);
    const CompiledSim sim = io::load_sim(args.positional[1]);
    io::check_pairing(*ship, sim);
    const io::Trace ref = io::load_trace(args.positional[2]);
    if (ref.sim != sim.id || ref.ship != ship->id)
      std::printf("warning: reference trace is for %s/%s, checking %s/%s\n", ref.ship.c_str(), ref.sim.c_str(), ship->id.c_str(), sim.id.c_str());
    if (ref.nodes != ship->nodes()) throw std::runtime_error("reference trace node count differs from the ship");
    if (ref.dt != sim.dt) throw std::runtime_error("reference trace dt differs from the simulation");
    std::printf("sinksim_check: %s / %s with %s numerics against %s\n  reference produced by %s, %s\n", ship->id.c_str(), sim.id.c_str(), numerics_name(numerics),
                args.positional[2].c_str(), ref.producer.c_str(), ref.engine.c_str());
    Simulation S(ship, sim, numerics);

    // 0. the initial record must be the compiled initial state
    {
      const Diff d0 = compare(S, ref.dense, 0);
      std::printf("%-20s %5d   max|diff| %.3e%s\n", "initial state", 1, d0.worst, d0.worst > 0 ? (" (" + d0.where + ")").c_str() : " (exact)");
    }
    // 1. dense single steps
    Worst dense;
    for (Index i = 0; i + 1 < ref.dense.n; ++i) {
      S.restore(ref.dense.snapshot(i, ref.nodes));
      S.step();
      dense.merge(compare(S, ref.dense, i + 1), "step " + std::to_string(i + 1));
    }
    print_worst("dense single steps", dense, ref.dense.n - 1);
    // 2. sampled windows
    Worst win;
    for (Index i = 0; i + 1 < ref.samples.n; ++i) {
      S.restore(ref.samples.snapshot(i, ref.nodes));
      const auto k = static_cast<std::size_t>(i);
      const long steps = std::lround((ref.samples.t[k + 1] - ref.samples.t[k]) / ref.dt);
      for (long s = 0; s < steps; ++s) S.step();
      win.merge(compare(S, ref.samples, i + 1), "window " + std::to_string(i) + "->" + std::to_string(i + 1) + " (t " + cli::minutes(ref.samples.t[k + 1]) + ")");
    }
    print_worst("sampled windows", win, ref.samples.n - 1);
    // 3. end to end
    S.reset_to_initial();
    const io::Trace mine = S.record_trace(5 * 3600, 0, ref.sampleEvery);
    const Index ns = std::min(mine.samples.n, ref.samples.n);
    std::printf("end to end: reference %s, engine %s; samples %d vs %d; events %zu vs %zu\n",
                ref.foundered ? ("founders at " + cli::minutes(ref.founderT)).c_str() : "afloat", mine.foundered ? ("founders at " + cli::minutes(mine.founderT)).c_str() : "afloat",
                ref.samples.n, mine.samples.n, ref.events.size(), mine.events.size());
    double maxEventShift = 0;
    bool eventIdsMatch = ref.events.size() == mine.events.size();
    for (std::size_t i = 0; i < std::min(ref.events.size(), mine.events.size()); ++i) {
      if (ref.events[i].id != mine.events[i].id) eventIdsMatch = false;
      const double shift = mine.events[i].t - ref.events[i].t;
      maxEventShift = std::fmax(maxEventShift, std::fabs(shift));
      if (!quiet) std::printf("  %-10s ref %9.2f s  engine %9.2f s  %+8.2f s%s\n", ref.events[i].id.c_str(), ref.events[i].t, mine.events[i].t, shift, ref.events[i].id != mine.events[i].id ? ("  (ID MISMATCH: " + mine.events[i].id + ")").c_str() : "");
    }
    double endWorst = 0;
    {
      const Index N = ref.nodes;
      if (!quiet) std::printf("  divergence profile:   sample  t_min   |d zO| m      |d pitch| deg  |d roll| deg   sum|d vol| m3   max|d vol| m3 (node)\n");
      for (Index i = 0; i < ns; ++i) {
        const auto k = static_cast<std::size_t>(i);
        const double dz = std::fabs(mine.samples.zO[k] - ref.samples.zO[k]);
        const double dp = std::fabs(mine.samples.pitch[k] - ref.samples.pitch[k]) * 180 / 3.141592653589793;
        const double dr = std::fabs(mine.samples.roll[k] - ref.samples.roll[k]) * 180 / 3.141592653589793;
        double sv = 0, mv = 0;
        Index nv = 0;
        for (Index n = 0; n < N; ++n) {
          const double d = std::fabs(mine.samples.vol[k * static_cast<std::size_t>(N) + static_cast<std::size_t>(n)] - ref.samples.vol[k * static_cast<std::size_t>(N) + static_cast<std::size_t>(n)]);
          sv += d;
          if (d > mv) { mv = d; nv = n; }
        }
        endWorst = std::fmax(endWorst, std::fmax(dz, mv));
        if (!quiet && (i < 12 || i % 10 == 0 || i >= ns - 3))
          std::printf("  %25d %6.1f   %10.3e   %12.3e   %12.3e   %13.3e   %13.3e (%d)\n", i, ref.samples.t[k] / 60, dz, dp, dr, sv, mv, nv);
      }
      std::printf("  end to end: largest sample difference %.3e (heave in m or node volume in m3)\n", endWorst);
    }
    const double founderShift = (ref.foundered && mine.foundered) ? std::fabs(mine.founderT - ref.founderT) : (ref.foundered == mine.foundered ? 0 : 1e9);
    const bool ok = dense.d.worst <= tolStep && win.d.worst <= tolWindow && founderShift <= tolFounder && maxEventShift <= tolEvent && eventIdsMatch;
    std::printf("RESULT: %s   (step %.1e <= %.0e, window %.1e <= %.0e, founder shift %.2f s <= %.0f, event shift %.2f s <= %.0f, event ids %s)\n", ok ? "PASS" : "FAIL",
                dense.d.worst, tolStep, win.d.worst, tolWindow, founderShift, tolFounder, maxEventShift, tolEvent, eventIdsMatch ? "match" : "DIFFER");
    return ok ? 0 : 1;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "sinksim_check: %s\n", e.what());
    return 1;
  }
}
