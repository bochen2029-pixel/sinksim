#include "sinksim/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "sinksim/kernel/body.hpp"
#include "sinksim/kernel/frame.hpp"
#include "sinksim/kernel/hydro.hpp"
#include "sinksim/kernel/levels.hpp"
#include "sinksim/kernel/reduce.hpp"
#include "sinksim/kernel/step.hpp"

namespace sinksim {

const char* numerics_name(Numerics n) {
  switch (n) {
    case Numerics::Oracle: return "oracle";
    case Numerics::Portable: return "portable";
    case Numerics::PortableMixed: return "portable32";
    case Numerics::Std: return "std";
  }
  return "?";
}

bool parse_numerics(const std::string& s, Numerics& out) {
  if (s == "oracle") { out = Numerics::Oracle; return true; }
  if (s == "portable") { out = Numerics::Portable; return true; }
  if (s == "portable32") { out = Numerics::PortableMixed; return true; }
  if (s == "std") { out = Numerics::Std; return true; }
  return false;
}

Simulation::Simulation(std::shared_ptr<const CompiledShip> ship, const CompiledSim& sim, Numerics numerics)
    : ship_(std::move(ship)), sim_(sim), inc_(build_incidence(sim)), numerics_(numerics) {
  if (!ship_) throw std::invalid_argument("Simulation: null ship");
  nodes_ = ship_->nodes();
  if (sim_.nodes() != nodes_) throw std::invalid_argument("Simulation: ship and simulation disagree on the node count");
  const auto nn = static_cast<std::size_t>(nodes_);
  vol_.assign(nn, 0); level_.assign(nn, 0);
  area_.assign(nn, 0); cx_.assign(nn, 0); cy_.assign(nn, 0); cz_.assign(nn, 0);
  zmin_.assign(nn, 0); zmax_.assign(nn, 0); hEff_.assign(nn, 0); aEff_.assign(nn, 0); acc_.assign(nn, 0); excess_.assign(nn, 0);
  deg_.assign(nn, 0);
  merged_.assign(static_cast<std::size_t>(sim_.groups()), 0);
  passes_.assign(static_cast<std::size_t>(sim_.maxGroupSize()), NodePass{});
  over_.assign(static_cast<std::size_t>(sim_.monitors()), 0);
  q_.assign(static_cast<std::size_t>(sim_.connections()), 0);
  dV_.assign(static_cast<std::size_t>(sim_.connections()), 0);
  evItems_.assign(static_cast<std::size_t>(sim_.monitors() + sim_.marks() + 1), Event{});
  overT_.assign(static_cast<std::size_t>(sim_.monitors()), -1);
  markT_.assign(static_cast<std::size_t>(sim_.marks()), -1);
  bind();
  reset_to_initial();
}

void Simulation::bind() {
  shipView_ = ship_->view();
  simView_.phys = sim_.phys;
  simView_.dt = sim_.dt;
  simView_.groups = GroupsView{sim_.groups(), sim_.gNodeStart.data(), sim_.gNodeCount.data(), sim_.gNodes.data(), sim_.gMode.data(),
                               sim_.gpx.data(), sim_.gpy.data(), sim_.gpz.data(), sim_.gMargin};
  simView_.conns = ConnectionsView{sim_.connections(), sim_.ca.data(), sim_.cb.data(), sim_.cLaw.data(), sim_.cx.data(), sim_.cy.data(),
                                   sim_.cz.data(), sim_.cArea.data(), sim_.cCoef.data(), sim_.cKind.data(), sim_.cIdx.data(), sim_.cEn.data(),
                                   sim_.cTOn.data(), sim_.cSkipGroup.data(), sim_.cMonitor.data(), q_.data()};
  simView_.inc = inc_.view();
  simView_.monitors = MonitorsView{sim_.monitors(), sim_.monitorThreshold};
  simView_.marks = MarksView{sim_.marks(), sim_.mkx.data(), sim_.mky.data(), sim_.mkz.data(), sim_.mkWhen.data()};
  simView_.founder = sim_.founder;
  simView_.body = sim_.body;

  st_.vol = vol_.data();
  st_.level = level_.data();

  sc_ = Scratch{area_.data(), cx_.data(), cy_.data(), cz_.data(), zmin_.data(), zmax_.data(), hEff_.data(), aEff_.data(),
                acc_.data(), excess_.data(), deg_.data(), merged_.data(), passes_.data(), over_.data(), dV_.data()};

  ev_.items = evItems_.data();
  ev_.capacity = static_cast<Index>(evItems_.size());
  ev_.overT = overT_.data();
  ev_.markT = markT_.data();
}

void Simulation::reset_to_initial() {
  restore(sim_.initial());
  for (auto& q : q_) q = 0;
  for (auto& d : dV_) d = 0;
  for (auto& m : merged_) m = 0;
  for (auto& a : area_) a = 0;
  evaluate_hydro();
}

void Simulation::restore(const StateSnapshot& s) {
  st_.t = s.t; st_.zO = s.zO; st_.pitch = s.pitch; st_.roll = s.roll;
  st_.vz = s.vz; st_.wth = s.wth; st_.wph = s.wph;
  for (Index n = 0; n < nodes_; ++n) {
    const auto i = static_cast<std::size_t>(n);
    vol_[i] = s.vol[n]; level_[i] = s.level[n];
    cx_[i] = s.cx[n]; cy_[i] = s.cy[n]; cz_[i] = s.cz[n];
  }
  reset_events();
}

void Simulation::reset_events() {
  ev_.count = 0;
  ev_.foundered = 0;
  ev_.founderT = -1;
  for (auto& t : overT_) t = -1;
  for (auto& t : markT_) t = -1;
}

Frame Simulation::frame_now() const {
  switch (numerics_) {
    case Numerics::Oracle: return kernel::pose_frame<OracleMath>(simView_.body, st_.zO, st_.pitch, st_.roll);
    case Numerics::Portable:
    case Numerics::PortableMixed: return kernel::pose_frame<PortableMath>(simView_.body, st_.zO, st_.pitch, st_.roll);
    case Numerics::Std: return kernel::pose_frame<StdMath>(simView_.body, st_.zO, st_.pitch, st_.roll);
  }
  return kernel::pose_frame<OracleMath>(simView_.body, st_.zO, st_.pitch, st_.roll);
}

Buoyancy Simulation::hydro_at(const Frame& F) const {
  switch (numerics_) {
    case Numerics::Portable: return kernel::hydro_pass_ordered<kernel::Order::Tree>(shipView_.cols, F);
    case Numerics::PortableMixed: return kernel::hydro_pass_ordered<kernel::Order::TreeMixed>(shipView_.cols, F);
    default: return kernel::hydro_pass_ordered<kernel::Order::Serial>(shipView_.cols, F);
  }
}

void Simulation::solve_levels_full(const Frame& F) {
  switch (numerics_) {
    case Numerics::Portable: kernel::solve_levels_ordered<kernel::Order::Tree>(shipView_, simView_.groups, F, true, st_, sc_); break;
    case Numerics::PortableMixed: kernel::solve_levels_ordered<kernel::Order::TreeMixed>(shipView_, simView_.groups, F, true, st_, sc_); break;
    default: kernel::solve_levels_ordered<kernel::Order::Serial>(shipView_, simView_.groups, F, true, st_, sc_); break;
  }
}

void Simulation::evaluate_hydro() {
  const Buoyancy hb = hydro_at(frame_now());
  out_.inflow = 0;
  out_.Vb = hb.V; out_.hullTop = hb.top;
  out_.bx = hb.x; out_.by = hb.y; out_.bz = hb.z;
  Real mw = 0;
  for (Index n = 0; n < nodes_; ++n) mw += simView_.phys.rho * vol_[static_cast<std::size_t>(n)];
  out_.mw = mw;
}

void Simulation::step() {
  using kernel::Order;
  switch (numerics_) {
    case Numerics::Oracle: kernel::step<OracleMath, Order::Serial>(shipView_, simView_, st_, sc_, out_, ev_); break;
    case Numerics::Portable: kernel::step<PortableMath, Order::Tree>(shipView_, simView_, st_, sc_, out_, ev_); break;
    case Numerics::PortableMixed: kernel::step<PortableMath, Order::TreeMixed>(shipView_, simView_, st_, sc_, out_, ev_); break;
    case Numerics::Std: kernel::step<StdMath, Order::Serial>(shipView_, simView_, st_, sc_, out_, ev_); break;
  }
}

kernel::Readouts Simulation::readouts() const {
  switch (numerics_) {
    case Numerics::Oracle: return kernel::readouts<OracleMath>(shipView_, simView_, sim_.readout, st_, out_);
    case Numerics::Portable:
    case Numerics::PortableMixed: return kernel::readouts<PortableMath>(shipView_, simView_, sim_.readout, st_, out_);
    case Numerics::Std: return kernel::readouts<StdMath>(shipView_, simView_, sim_.readout, st_, out_);
  }
  return kernel::readouts<OracleMath>(shipView_, simView_, sim_.readout, st_, out_);
}

RunResult Simulation::run(const RunOptions& opt, const std::function<bool(Simulation&)>& onStep) {
  RunResult res;
  Real next = 0, calm = 0;
  const Real dt = sim_.dt;
  auto record = [&]() {
    const kernel::Readouts r = readouts();
    res.hist.push_back(HistoryRecord{r.t, r.trimDeg, r.listDeg, r.waterT, r.inflowTpm, r.draftF, r.draftA});
  };
  while (st_.t < opt.tMax && !ev_.foundered) {
    if (st_.t >= next) { record(); next += opt.every; }
    step();
    res.steps++;
    if (opt.stopWhenStable && st_.t > 1800) {
      calm = (std::fabs(out_.inflow) < 0.02 && std::fabs(st_.wth) < 1e-5 && std::fabs(st_.vz) < 1e-3) ? calm + dt : 0;
      if (calm > 1200) break;
    }
    if (onStep && !onStep(*this)) break;
  }
  record();
  res.final = readouts();
  res.foundered = ev_.foundered != 0;
  res.founderT = ev_.founderT;
  res.tEnd = st_.t;
  res.events = events();
  return res;
}

io::Trace Simulation::record_trace(Real tMax, int denseSteps, Real sampleEvery) {
  io::Trace tr;
  tr.ship = ship_->id;
  tr.sim = sim_.id;
  tr.title = sim_.title;
  tr.producer = std::string("sinksim CPU engine, numerics ") + numerics_name(numerics_);
  tr.dt = sim_.dt;
  tr.nodes = nodes_;
  tr.denseSteps = denseSteps;
  tr.sampleEvery = sampleEvery;
  evaluate_hydro();
  tr.dense.push(st_, out_, sc_, nodes_);
  tr.samples.push(st_, out_, sc_, nodes_);
  Real next = sampleEvery;
  std::int64_t steps = 0;
  while (!ev_.foundered && st_.t < tMax) {
    step();
    ++steps;
    if (steps <= denseSteps) tr.dense.push(st_, out_, sc_, nodes_);
    if (st_.t >= next - 1e-9) { tr.samples.push(st_, out_, sc_, nodes_); next += sampleEvery; }
  }
  tr.steps = steps;
  tr.tEnd = st_.t;
  tr.foundered = ev_.foundered != 0;
  tr.founderT = ev_.founderT;
  for (const Event& e : events()) tr.events.push_back(io::TraceEvent{e.t, event_id(e), event_label(e)});
  return tr;
}

std::vector<Event> Simulation::events() const { return std::vector<Event>(evItems_.begin(), evItems_.begin() + ev_.count); }

std::string Simulation::event_id(const Event& e) const {
  switch (e.kind) {
    case EventKind::Overflow: return sim_.monitorId[static_cast<std::size_t>(e.id)];
    case EventKind::Mark: return sim_.markId[static_cast<std::size_t>(e.id)];
    case EventKind::Founder: return "founder";
  }
  return "?";
}

std::string Simulation::event_label(const Event& e) const {
  switch (e.kind) {
    case EventKind::Overflow: return sim_.monitorLabel[static_cast<std::size_t>(e.id)];
    case EventKind::Mark: return sim_.markLabel[static_cast<std::size_t>(e.id)];
    case EventKind::Founder: return "Foundered";
  }
  return "?";
}

void Simulation::set_connection_enabled(Index c, bool en) { sim_.cEn[static_cast<std::size_t>(c)] = en ? 1 : 0; }
void Simulation::set_connection_area(Index c, Real area) { sim_.cArea[static_cast<std::size_t>(c)] = area; }
void Simulation::set_connection_activation(Index c, Real tOn) { sim_.cTOn[static_cast<std::size_t>(c)] = tOn; }

Index Simulation::set_kind_enabled(Byte kind, Index idx, bool en) {
  Index count = 0;
  for (Index c = 0; c < sim_.connections(); ++c) {
    const auto i = static_cast<std::size_t>(c);
    if (sim_.cKind[i] == kind && sim_.cIdx[i] == idx) { sim_.cEn[i] = en ? 1 : 0; ++count; }
  }
  return count;
}

Real Simulation::gm_now() {
  // The oracle restores level, centroids and area but not the volumes, which a merged group redistributes.
  // The engine restores everything, so this diagnostic has no side effect. Restores copy element-wise so
  // that the kernel views, which point into these vectors, stay valid.
  const std::vector<Real> vol = vol_, level = level_, cx = cx_, cy = cy_, cz = cz_, area = area_, zmin = zmin_, zmax = zmax_;
  const std::vector<Byte> merged = merged_;
  const Real d = 0.01;
  Real out[2];
  const Real roll0 = st_.roll;
  for (int s = 0; s < 2; ++s) {
    const Real sign = s == 0 ? -1 : 1;
    st_.roll = roll0 + sign * d;
    const Frame F = frame_now();
    st_.roll = roll0;
    const Buoyancy hb = hydro_at(F);
    solve_levels_full(F);
    const Loads L = kernel::loads(shipView_, simView_, F, hb, st_, sc_);
    out[s] = L.Mph;
    std::copy(vol.begin(), vol.end(), vol_.begin());
    std::copy(level.begin(), level.end(), level_.begin());
    std::copy(cx.begin(), cx.end(), cx_.begin());
    std::copy(cy.begin(), cy.end(), cy_.begin());
    std::copy(cz.begin(), cz.end(), cz_.begin());
    std::copy(area.begin(), area.end(), area_.begin());
    std::copy(zmin.begin(), zmin.end(), zmin_.begin());
    std::copy(zmax.begin(), zmax.end(), zmax_.begin());
    std::copy(merged.begin(), merged.end(), merged_.begin());
  }
  Real w = simView_.body.ms;
  for (Index n = 0; n < nodes_; ++n) w += simView_.phys.rho * vol_[static_cast<std::size_t>(n)];
  return -(out[1] - out[0]) / (2 * d) / (w * simView_.phys.g);
}

}  // namespace sinksim
