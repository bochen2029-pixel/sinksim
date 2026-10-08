#include "sinksim/sweep.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <stdexcept>
#include <thread>

#include "sinksim/io/json.hpp"

namespace sinksim::sweep {

using json::Json;

namespace {

Byte kind_byte(const CompiledSim& sim, const std::string& name, const std::string& where) {
  const Index k = sim.kindIndex(name);
  if (k < 0) throw std::runtime_error(where + ": unknown connection kind \"" + name + "\"");
  return static_cast<Byte>(k);
}

std::string kind_name(const CompiledSim& sim, Byte kind) {
  const auto i = static_cast<std::size_t>(kind);
  return i < sim.kindNames.size() ? sim.kindNames[i] : std::to_string(kind);
}

}  // namespace

Sweep load_sweep(const std::string& path, const CompiledSim& sim) {
  const Json doc = json::read_file(path);
  if (json::string_or(doc, "format", "") != "sinksim.sweep") throw std::runtime_error(path + ": format is not sinksim.sweep");
  if (json::integer(doc, "formatVersion", path) != 1) throw std::runtime_error(path + ": unsupported sweep formatVersion");
  Sweep sw;
  sw.ship = json::string_or(doc, "ship", "");
  sw.sim = json::string_or(doc, "sim", "");
  sw.tMax = json::number_or(doc, "tMax", 5 * 3600);
  sw.every = json::number_or(doc, "every", 30);
  sw.stopWhenStable = doc.contains("stopWhenStable") && doc["stopWhenStable"].is_boolean() && doc["stopWhenStable"].get<bool>();
  const Json& list = json::member(doc, "instances", path);
  if (!list.is_array()) throw std::runtime_error(path + ": instances must be an array");
  std::size_t k = 0;
  for (const Json& j : list) {
    Instance in;
    in.id = json::string_or(j, "id", "instance" + std::to_string(k));
    const std::string where = path + " instance " + in.id;
    if (j.contains("scale")) {
      const Json& s = j["scale"];
      if (!s.is_object()) throw std::runtime_error(where + ": scale must be an object of kind -> factor");
      for (auto it = s.begin(); it != s.end(); ++it) {
        if (!it.value().is_number()) throw std::runtime_error(where + ": scale factor for " + it.key() + " is not a number");
        in.kindScale.emplace_back(kind_byte(sim, it.key(), where), it.value().get<double>());
      }
    }
    if (j.contains("connections")) {
      const Json& list2 = j["connections"];
      if (!list2.is_array()) throw std::runtime_error(where + ": connections must be an array");
      for (const Json& o : list2) {
        ConnectionOverride ov;
        if (o.contains("conn")) ov.conn = static_cast<Index>(json::integer(o, "conn", where));
        else {
          ov.kind = kind_byte(sim, json::string(o, "kind", where), where);
          if (o.contains("idx")) ov.idx = static_cast<Index>(json::integer(o, "idx", where));
        }
        if (o.contains("enabled")) { ov.hasEnabled = true; ov.enabled = json::boolean(o, "enabled", where); }
        if (o.contains("tOn")) { ov.hasTOn = true; ov.tOn = json::number(o, "tOn", where); }
        if (o.contains("area")) { ov.hasArea = true; ov.area = json::number(o, "area", where); }
        in.overrides.push_back(ov);
      }
    }
    sw.instances.push_back(std::move(in));
    ++k;
  }
  return sw;
}

void save_sweep(const Sweep& sw, const std::string& path, const CompiledSim& sim) {
  Json doc = Json::object();
  doc["format"] = "sinksim.sweep";
  doc["formatVersion"] = 1;
  doc["ship"] = sw.ship;
  doc["sim"] = sw.sim;
  doc["tMax"] = sw.tMax;
  doc["every"] = sw.every;
  doc["stopWhenStable"] = sw.stopWhenStable;
  Json list = Json::array();
  for (const Instance& in : sw.instances) {
    Json j = Json::object();
    j["id"] = in.id;
    if (!in.kindScale.empty()) {
      Json s = Json::object();
      for (const auto& [kind, factor] : in.kindScale) s[kind_name(sim, kind)] = factor;
      j["scale"] = s;
    }
    if (!in.overrides.empty()) {
      Json list2 = Json::array();
      for (const ConnectionOverride& o : in.overrides) {
        Json x = Json::object();
        if (o.conn >= 0) x["conn"] = o.conn;
        else { x["kind"] = kind_name(sim, o.kind); if (o.idx >= 0) x["idx"] = o.idx; }
        if (o.hasEnabled) x["enabled"] = o.enabled;
        if (o.hasTOn) x["tOn"] = o.tOn;
        if (o.hasArea) x["area"] = o.area;
        list2.push_back(x);
      }
      j["connections"] = list2;
    }
    list.push_back(j);
  }
  doc["instances"] = list;
  json::write_file(path, doc, 2);
}

void apply(const Instance& in, const CompiledSim& base, Simulation& S) {
  apply(in, base,
        [&](Index c, bool en) { S.set_connection_enabled(c, en); },
        [&](Index c, Real a) { S.set_connection_area(c, a); },
        [&](Index c, Real t) { S.set_connection_activation(c, t); });
}

Results run_cpu(std::shared_ptr<const CompiledShip> ship, const CompiledSim& sim, const Sweep& sw, Numerics numerics, int threads) {
  Results out;
  out.ship = ship->id;
  out.sim = sim.id;
  out.numerics = numerics_name(numerics);
  out.engine = "sinksim CPU engine";
  out.tMax = sw.tMax;
  out.every = sw.every;
  out.instances.resize(sw.instances.size());
  if (threads <= 0) threads = static_cast<int>(std::thread::hardware_concurrency());
  if (threads <= 0) threads = 1;
  std::atomic<std::size_t> next{0};
  auto worker = [&]() {
    for (;;) {
      const std::size_t i = next.fetch_add(1);
      if (i >= sw.instances.size()) return;
      const Instance& in = sw.instances[i];
      Simulation S(ship, sim, numerics);
      apply(in, sim, S);
      RunOptions opt;
      opt.tMax = sw.tMax;
      opt.every = sw.every;
      opt.stopWhenStable = sw.stopWhenStable;
      const RunResult r = S.run(opt);
      InstanceResult& res = out.instances[i];
      res.id = in.id;
      res.foundered = r.foundered;
      res.founderT = r.founderT;
      res.tEnd = r.tEnd;
      res.steps = r.steps;
      res.final = r.final;
      for (const Event& e : r.events) res.events.push_back(EventRecord{e.t, S.event_id(e), S.event_label(e)});
      for (const HistoryRecord& h : r.hist) res.curve.push_back(CurveRecord{h.t, h.trim, h.list, h.water, h.inflow, h.dF, h.dA});
    }
  };
  std::vector<std::thread> pool;
  const int n = std::min<int>(threads, static_cast<int>(sw.instances.size()));
  for (int t = 0; t < n; ++t) pool.emplace_back(worker);
  for (std::thread& t : pool) t.join();
  return out;
}

void save_results(const Results& r, const std::string& path) {
  Json doc = Json::object();
  doc["format"] = "sinksim.batch-results";
  doc["formatVersion"] = 1;
  doc["ship"] = r.ship;
  doc["sim"] = r.sim;
  doc["numerics"] = r.numerics;
  doc["engine"] = r.engine;
  doc["tMax"] = r.tMax;
  doc["every"] = r.every;
  Json list = Json::array();
  for (const InstanceResult& in : r.instances) {
    Json j = Json::object();
    j["id"] = in.id;
    j["foundered"] = in.foundered;
    j["founderT"] = in.founderT;
    j["tEnd"] = in.tEnd;
    j["steps"] = in.steps;
    Json f = Json::object();
    f["trimDeg"] = in.final.trimDeg; f["listDeg"] = in.final.listDeg; f["waterT"] = in.final.waterT;
    f["draftF"] = in.final.draftF; f["draftA"] = in.final.draftA; f["inflowTpm"] = in.final.inflowTpm;
    j["final"] = f;
    Json ev = Json::array();
    for (const EventRecord& e : in.events) {
      Json x = Json::object();
      x["t"] = e.t; x["id"] = e.id; x["label"] = e.label;
      ev.push_back(x);
    }
    j["events"] = ev;
    Json c = Json::object();
    std::vector<Real> t, trim, list2, water, inflow, dF, dA;
    for (const CurveRecord& k : in.curve) { t.push_back(k.t); trim.push_back(k.trim); list2.push_back(k.list); water.push_back(k.water); inflow.push_back(k.inflow); dF.push_back(k.dF); dA.push_back(k.dA); }
    c["t"] = t; c["trim"] = trim; c["list"] = list2; c["water"] = water; c["inflow"] = inflow; c["draftF"] = dF; c["draftA"] = dA;
    j["curve"] = c;
    list.push_back(j);
  }
  doc["instances"] = list;
  json::write_file(path, doc, 2);
}

Results load_results(const std::string& path) {
  const Json doc = json::read_file(path);
  if (json::string_or(doc, "format", "") != "sinksim.batch-results") throw std::runtime_error(path + ": format is not sinksim.batch-results");
  Results r;
  r.ship = json::string_or(doc, "ship", "");
  r.sim = json::string_or(doc, "sim", "");
  r.numerics = json::string_or(doc, "numerics", "");
  r.engine = json::string_or(doc, "engine", "");
  r.tMax = json::number_or(doc, "tMax", 0);
  r.every = json::number_or(doc, "every", 0);
  for (const Json& j : json::member(doc, "instances", path)) {
    InstanceResult in;
    in.id = json::string_or(j, "id", "");
    in.foundered = json::boolean(j, "foundered", path);
    in.founderT = json::number(j, "founderT", path);
    in.tEnd = json::number(j, "tEnd", path);
    in.steps = json::integer(j, "steps", path);
    const Json& f = json::member(j, "final", path);
    in.final.trimDeg = json::number(f, "trimDeg", path); in.final.listDeg = json::number(f, "listDeg", path); in.final.waterT = json::number(f, "waterT", path);
    in.final.draftF = json::number(f, "draftF", path); in.final.draftA = json::number(f, "draftA", path); in.final.inflowTpm = json::number(f, "inflowTpm", path);
    in.final.t = in.tEnd;
    for (const Json& e : json::member(j, "events", path)) in.events.push_back(EventRecord{json::number(e, "t", path), json::string(e, "id", path), json::string_or(e, "label", "")});
    const Json& c = json::member(j, "curve", path);
    const auto col = [&](const char* k) { std::vector<Real> v; for (const Json& x : json::member(c, k, path)) v.push_back(x.get<double>()); return v; };
    const std::vector<Real> t = col("t"), trim = col("trim"), list = col("list"), water = col("water"), inflow = col("inflow"), dF = col("draftF"), dA = col("draftA");
    for (std::size_t i = 0; i < t.size(); ++i) in.curve.push_back(CurveRecord{t[i], trim[i], list[i], water[i], inflow[i], dF[i], dA[i]});
    r.instances.push_back(std::move(in));
  }
  return r;
}

Diff compare(const Results& a, const Results& b) {
  Diff d;
  auto note = [&](const std::string& where, double x, double y) {
    ++d.compared;
    if (x != y && !(std::isnan(x) && std::isnan(y))) {
      ++d.differing;
      const double ad = std::fabs(x - y);
      if (ad > d.maxAbs) d.maxAbs = ad;
      if (d.first.empty()) d.first = where + ": " + json::format_number(x) + " vs " + json::format_number(y);
    }
  };
  if (a.instances.size() != b.instances.size()) {
    d.differing++;
    d.first = "instance count " + std::to_string(a.instances.size()) + " vs " + std::to_string(b.instances.size());
    return d;
  }
  for (std::size_t i = 0; i < a.instances.size(); ++i) {
    const InstanceResult& x = a.instances[i];
    const InstanceResult& y = b.instances[i];
    const std::string w = "instance " + x.id;
    note(w + " foundered", x.foundered, y.foundered);
    note(w + " founderT", x.founderT, y.founderT);
    note(w + " tEnd", x.tEnd, y.tEnd);
    note(w + " steps", static_cast<double>(x.steps), static_cast<double>(y.steps));
    note(w + " trim", x.final.trimDeg, y.final.trimDeg);
    note(w + " list", x.final.listDeg, y.final.listDeg);
    note(w + " water", x.final.waterT, y.final.waterT);
    note(w + " draftF", x.final.draftF, y.final.draftF);
    note(w + " draftA", x.final.draftA, y.final.draftA);
    note(w + " events", static_cast<double>(x.events.size()), static_cast<double>(y.events.size()));
    for (std::size_t k = 0; k < std::min(x.events.size(), y.events.size()); ++k) {
      note(w + " event " + x.events[k].id + " t", x.events[k].t, y.events[k].t);
      if (x.events[k].id != y.events[k].id) { ++d.differing; if (d.first.empty()) d.first = w + " event id " + x.events[k].id + " vs " + y.events[k].id; }
    }
    note(w + " curve length", static_cast<double>(x.curve.size()), static_cast<double>(y.curve.size()));
    for (std::size_t k = 0; k < std::min(x.curve.size(), y.curve.size()); ++k) {
      const std::string c = w + " curve[" + std::to_string(k) + "]";
      note(c + " t", x.curve[k].t, y.curve[k].t);
      note(c + " trim", x.curve[k].trim, y.curve[k].trim);
      note(c + " list", x.curve[k].list, y.curve[k].list);
      note(c + " water", x.curve[k].water, y.curve[k].water);
      note(c + " inflow", x.curve[k].inflow, y.curve[k].inflow);
      note(c + " draftF", x.curve[k].dF, y.curve[k].dF);
      note(c + " draftA", x.curve[k].dA, y.curve[k].dA);
    }
  }
  return d;
}

}  // namespace sinksim::sweep
