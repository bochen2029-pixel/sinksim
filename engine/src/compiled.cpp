#include "sinksim/io/compiled.hpp"

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

#include "sinksim/hash.hpp"
#include "sinksim/io/json.hpp"

namespace sinksim::io {

namespace {

using json::Json;

[[noreturn]] void bad(const std::string& file, const std::string& what) { throw std::runtime_error(file + ": " + what); }

const Json& at(const Json& obj, const char* key, const std::string& file) { return json::member(obj, key, file); }

std::vector<Real> numbers(const Json& v, const std::string& file, const std::string& what) {
  if (!v.is_array()) bad(file, what + " must be an array");
  std::vector<Real> out;
  out.reserve(v.size());
  std::size_t i = 0;
  for (const Json& x : v) {
    if (!x.is_number()) bad(file, what + "[" + std::to_string(i) + "] is not a number");
    out.push_back(x.get<double>());
    ++i;
  }
  return out;
}

std::vector<Index> indices(const Json& v, const std::string& file, const std::string& what, Index lo, Index hi) {
  const std::vector<Real> d = numbers(v, file, what);
  std::vector<Index> out;
  out.reserve(d.size());
  for (std::size_t i = 0; i < d.size(); ++i) {
    if (d[i] != std::floor(d[i]) || d[i] < lo || d[i] > hi)
      bad(file, what + "[" + std::to_string(i) + "] = " + json::format_number(d[i]) + " is not an integer in [" + std::to_string(lo) + ", " + std::to_string(hi) + "]");
    out.push_back(static_cast<Index>(d[i]));
  }
  return out;
}

std::vector<Byte> bytes(const Json& v, const std::string& file, const std::string& what) {
  const std::vector<Index> d = indices(v, file, what, 0, 255);
  std::vector<Byte> out;
  out.reserve(d.size());
  for (const Index x : d) out.push_back(static_cast<Byte>(x));
  return out;
}

std::vector<std::string> strings(const Json& v, const std::string& file, const std::string& what) {
  if (!v.is_array()) bad(file, what + " must be an array");
  std::vector<std::string> out;
  for (const Json& s : v) {
    if (!s.is_string()) bad(file, what + " must contain strings");
    out.push_back(s.get<std::string>());
  }
  return out;
}

void same_size(const std::string& file, const std::string& what, std::size_t got, std::size_t want) {
  if (got != want) bad(file, what + " has " + std::to_string(got) + " entries, expected " + std::to_string(want));
}

// One hashed array, in the order the compiler used (docs/FORMATS.md).
struct Hashed {
  const char* name;
  char kind;  // 'f' = f64, 'i' = i32, 'b' = u8
  const void* data;
  std::size_t n;
};

void feed(Fnv1a64& h, const Hashed& a) {
  if (a.kind == 'f') h.update_f64(static_cast<const double*>(a.data), a.n);
  else if (a.kind == 'i') h.update_i32(static_cast<const std::int32_t*>(a.data), a.n);
  else h.update_u8(static_cast<const std::uint8_t*>(a.data), a.n);
}

std::string verify_hashes(const Json& doc, const std::string& file, const std::vector<Hashed>& arrays) {
  const Json& H = at(doc, "hash", file);
  Fnv1a64 chain;
  for (const Hashed& a : arrays) {
    Fnv1a64 one;
    feed(one, a);
    feed(chain, a);
    const std::string want = json::string(H, a.name, file + " hash");
    if (one.hex() != want)
      bad(file, std::string("hash mismatch on ") + a.name + " (" + one.hex() + " vs " + want + "): the JSON round trip was not exact");
  }
  const std::string want = json::string(H, "all", file + " hash");
  if (chain.hex() != want) bad(file, "chained hash mismatch (" + chain.hex() + " vs " + want + ")");
  return chain.hex();
}

void check_format(const Json& doc, const std::string& file, const char* format, int version) {
  if (json::string_or(doc, "format", "") != format) bad(file, std::string("format is not ") + format);
  const long long v = json::integer(doc, "formatVersion", file);
  if (v != version) bad(file, "formatVersion " + std::to_string(v) + " is not supported (engine reads " + std::to_string(version) + ")");
}

Real real_at(const Json& obj, const char* key, const std::string& file) { return static_cast<Real>(json::number(obj, key, file)); }

}  // namespace

std::shared_ptr<CompiledShip> load_ship(const std::string& file) {
  const Json doc = json::read_file(file);
  check_format(doc, file, "sinksim.compiled-ship", kCompiledShipFormatVersion);
  auto S = std::make_shared<CompiledShip>();
  S->id = json::string(doc, "id", file);
  if (doc.contains("meta")) {
    const Json& meta = doc["meta"];
    S->title = json::string_or(meta, "title", "");
    S->producer = json::string_or(meta, "generator", "") + " on " + json::string_or(meta, "engine", "");
  }
  const Json& C = at(doc, "columns", file);
  const auto nc = static_cast<std::size_t>(json::integer(C, "n", file));
  S->x = numbers(at(C, "x", file), file, "columns.x"); same_size(file, "columns.x", S->x.size(), nc);
  S->y = numbers(at(C, "y", file), file, "columns.y"); same_size(file, "columns.y", S->y.size(), nc);
  S->dxdy = numbers(at(C, "dxdy", file), file, "columns.dxdy"); same_size(file, "columns.dxdy", S->dxdy.size(), nc);
  S->zlo = numbers(at(C, "zlo", file), file, "columns.zlo"); same_size(file, "columns.zlo", S->zlo.size(), nc);
  S->zhi = numbers(at(C, "zhi", file), file, "columns.zhi"); same_size(file, "columns.zhi", S->zhi.size(), nc);
  S->zone = indices(at(C, "zone", file), file, "columns.zone", 0, 1 << 30); same_size(file, "columns.zone", S->zone.size(), nc);
  S->side = indices(at(C, "side", file), file, "columns.side", 0, 1); same_size(file, "columns.side", S->side.size(), nc);

  const Json& N = at(doc, "nodes", file);
  const auto nn = static_cast<std::size_t>(json::integer(N, "n", file));
  S->nodeLabel = strings(at(N, "label", file), file, "nodes.label"); same_size(file, "nodes.label", S->nodeLabel.size(), nn);
  S->mu = numbers(at(N, "mu", file), file, "nodes.mu"); same_size(file, "nodes.mu", S->mu.size(), nn);
  S->vmax = numbers(at(N, "vmax", file), file, "nodes.vmax"); same_size(file, "nodes.vmax", S->vmax.size(), nn);
  S->amin = numbers(at(N, "amin", file), file, "nodes.amin"); same_size(file, "nodes.amin", S->amin.size(), nn);
  S->aov = numbers(at(N, "aov", file), file, "nodes.aov"); same_size(file, "nodes.aov", S->aov.size(), nn);

  const Json& G = at(doc, "segments", file);
  const auto ns = static_cast<std::size_t>(json::integer(G, "n", file));
  S->segStart = indices(at(N, "segStart", file), file, "nodes.segStart", 0, static_cast<Index>(ns)); same_size(file, "nodes.segStart", S->segStart.size(), nn);
  S->segCount = indices(at(N, "segCount", file), file, "nodes.segCount", 0, static_cast<Index>(ns)); same_size(file, "nodes.segCount", S->segCount.size(), nn);
  S->segCol = indices(at(G, "col", file), file, "segments.col", 0, static_cast<Index>(nc) - 1); same_size(file, "segments.col", S->segCol.size(), ns);
  S->segA = numbers(at(G, "a", file), file, "segments.a"); same_size(file, "segments.a", S->segA.size(), ns);
  S->segE = numbers(at(G, "e", file), file, "segments.e"); same_size(file, "segments.e", S->segE.size(), ns);
  for (std::size_t n = 0; n < nn; ++n)
    if (static_cast<std::size_t>(S->segStart[n]) + static_cast<std::size_t>(S->segCount[n]) > ns)
      bad(file, "node " + std::to_string(n) + " segment range exceeds the segment table");

  S->hashAll = verify_hashes(doc, file, {
    {"columns.x", 'f', S->x.data(), nc}, {"columns.y", 'f', S->y.data(), nc}, {"columns.dxdy", 'f', S->dxdy.data(), nc},
    {"columns.zlo", 'f', S->zlo.data(), nc}, {"columns.zhi", 'f', S->zhi.data(), nc},
    {"columns.zone", 'i', S->zone.data(), nc}, {"columns.side", 'i', S->side.data(), nc},
    {"nodes.mu", 'f', S->mu.data(), nn}, {"nodes.vmax", 'f', S->vmax.data(), nn}, {"nodes.amin", 'f', S->amin.data(), nn},
    {"nodes.aov", 'f', S->aov.data(), nn}, {"nodes.segStart", 'i', S->segStart.data(), nn}, {"nodes.segCount", 'i', S->segCount.data(), nn},
    {"segments.col", 'i', S->segCol.data(), ns}, {"segments.a", 'f', S->segA.data(), ns}, {"segments.e", 'f', S->segE.data(), ns},
  });
  return S;
}

CompiledSim load_sim(const std::string& file) {
  const Json doc = json::read_file(file);
  check_format(doc, file, "sinksim.compiled-sim", kCompiledSimFormatVersion);
  CompiledSim M;
  M.id = json::string(doc, "id", file);
  M.shipId = json::string(doc, "ship", file);
  M.shipHash = json::string(doc, "shipHash", file);
  if (doc.contains("meta")) {
    const Json& meta = doc["meta"];
    M.title = json::string_or(meta, "title", "");
    M.producer = json::string_or(meta, "generator", "") + " on " + json::string_or(meta, "engine", "");
  }
  const Json& K = at(doc, "constants", file);
  M.phys.rho = real_at(K, "rho", file);
  M.phys.g = real_at(K, "g", file);
  M.phys.sq2g = std::sqrt(2 * M.phys.g);
  M.dt = real_at(at(doc, "scheme", file), "dt", file);

  const Json& st = at(doc, "state0", file);
  M.t0 = real_at(st, "t", file); M.zO0 = real_at(st, "zO", file); M.pitch0 = real_at(st, "pitch", file); M.roll0 = real_at(st, "roll", file);
  M.vz0 = real_at(st, "vz", file); M.wth0 = real_at(st, "wth", file); M.wph0 = real_at(st, "wph", file);
  M.vol0 = numbers(at(st, "vol", file), file, "state0.vol");
  const auto nn = M.vol0.size();
  M.level0 = numbers(at(st, "level", file), file, "state0.level"); same_size(file, "state0.level", M.level0.size(), nn);
  M.cx0 = numbers(at(st, "cx", file), file, "state0.cx"); same_size(file, "state0.cx", M.cx0.size(), nn);
  M.cy0 = numbers(at(st, "cy", file), file, "state0.cy"); same_size(file, "state0.cy", M.cy0.size(), nn);
  M.cz0 = numbers(at(st, "cz", file), file, "state0.cz"); same_size(file, "state0.cz", M.cz0.size(), nn);

  const Json& G = at(doc, "groups", file);
  const auto ng = static_cast<std::size_t>(json::integer(G, "n", file));
  M.gNodeCount = indices(at(G, "nodeCount", file), file, "groups.nodeCount", 1, 1 << 20); same_size(file, "groups.nodeCount", M.gNodeCount.size(), ng);
  M.gNodes = indices(at(G, "nodes", file), file, "groups.nodes", 0, static_cast<Index>(nn) - 1);
  M.gNodeStart = indices(at(G, "nodeStart", file), file, "groups.nodeStart", 0, static_cast<Index>(M.gNodes.size())); same_size(file, "groups.nodeStart", M.gNodeStart.size(), ng);
  M.gMode = bytes(at(G, "mode", file), file, "groups.mode"); same_size(file, "groups.mode", M.gMode.size(), ng);
  M.gpx = numbers(at(G, "px", file), file, "groups.px"); same_size(file, "groups.px", M.gpx.size(), ng);
  M.gpy = numbers(at(G, "py", file), file, "groups.py"); same_size(file, "groups.py", M.gpy.size(), ng);
  M.gpz = numbers(at(G, "pz", file), file, "groups.pz"); same_size(file, "groups.pz", M.gpz.size(), ng);
  M.gMargin = real_at(G, "margin", file);
  for (std::size_t g = 0; g < ng; ++g)
    if (static_cast<std::size_t>(M.gNodeStart[g]) + static_cast<std::size_t>(M.gNodeCount[g]) > M.gNodes.size())
      bad(file, "group " + std::to_string(g) + " member range exceeds the member table");

  const Json& C = at(doc, "connections", file);
  const auto ncn = static_cast<std::size_t>(json::integer(C, "n", file));
  M.ca = indices(at(C, "a", file), file, "connections.a", -1, static_cast<Index>(nn) - 1); same_size(file, "connections.a", M.ca.size(), ncn);
  M.cb = indices(at(C, "b", file), file, "connections.b", 0, static_cast<Index>(nn) - 1); same_size(file, "connections.b", M.cb.size(), ncn);
  M.cLaw = bytes(at(C, "type", file), file, "connections.type"); same_size(file, "connections.type", M.cLaw.size(), ncn);
  M.cx = numbers(at(C, "x", file), file, "connections.x"); same_size(file, "connections.x", M.cx.size(), ncn);
  M.cy = numbers(at(C, "y", file), file, "connections.y"); same_size(file, "connections.y", M.cy.size(), ncn);
  M.cz = numbers(at(C, "z", file), file, "connections.z"); same_size(file, "connections.z", M.cz.size(), ncn);
  M.cArea = numbers(at(C, "area", file), file, "connections.area"); same_size(file, "connections.area", M.cArea.size(), ncn);
  M.cCoef = numbers(at(C, "coef", file), file, "connections.coef"); same_size(file, "connections.coef", M.cCoef.size(), ncn);
  M.cKind = bytes(at(C, "kind", file), file, "connections.kind"); same_size(file, "connections.kind", M.cKind.size(), ncn);
  M.cIdx = indices(at(C, "idx", file), file, "connections.idx", -1, 1 << 30); same_size(file, "connections.idx", M.cIdx.size(), ncn);
  M.cEn = bytes(at(C, "en", file), file, "connections.en"); same_size(file, "connections.en", M.cEn.size(), ncn);
  M.cTOn = numbers(at(C, "tOn", file), file, "connections.tOn"); same_size(file, "connections.tOn", M.cTOn.size(), ncn);
  M.cSkipGroup = indices(at(C, "skipGroup", file), file, "connections.skipGroup", -1, static_cast<Index>(ng) - 1); same_size(file, "connections.skipGroup", M.cSkipGroup.size(), ncn);
  M.cMonitor = indices(at(C, "monitor", file), file, "connections.monitor", -1, 1 << 30); same_size(file, "connections.monitor", M.cMonitor.size(), ncn);
  if (C.contains("kindNames")) M.kindNames = strings(C["kindNames"], file, "connections.kindNames");

  const Json& Mo = at(doc, "monitors", file);
  M.monitorThreshold = real_at(Mo, "threshold", file);
  M.monitorId = strings(at(Mo, "id", file), file, "monitors.id");
  M.monitorLabel = strings(at(Mo, "label", file), file, "monitors.label");
  same_size(file, "monitors.label", M.monitorLabel.size(), M.monitorId.size());
  for (std::size_t c = 0; c < ncn; ++c)
    if (M.cMonitor[c] >= static_cast<Index>(M.monitorId.size())) bad(file, "connection " + std::to_string(c) + " refers to a monitor that does not exist");

  const Json& Mk = at(doc, "marks", file);
  M.markId = strings(at(Mk, "id", file), file, "marks.id");
  M.markLabel = strings(at(Mk, "label", file), file, "marks.label");
  M.mkx = numbers(at(Mk, "x", file), file, "marks.x");
  M.mky = numbers(at(Mk, "y", file), file, "marks.y");
  M.mkz = numbers(at(Mk, "z", file), file, "marks.z");
  M.mkWhen = bytes(at(Mk, "when", file), file, "marks.when");
  same_size(file, "marks.label", M.markLabel.size(), M.markId.size());
  same_size(file, "marks.x", M.mkx.size(), M.markId.size());
  same_size(file, "marks.y", M.mky.size(), M.markId.size());
  same_size(file, "marks.z", M.mkz.size(), M.markId.size());
  same_size(file, "marks.when", M.mkWhen.size(), M.markId.size());

  const Json& Fo = at(doc, "founder", file);
  M.founder.hullTopBelow = real_at(Fo, "hullTopBelow", file);
  M.founder.pitchAbsAbove = real_at(Fo, "pitchAbsAbove", file);
  M.founder.buoyancyBelow = real_at(Fo, "buoyancyBelow", file);
  M.founder.minT = real_at(Fo, "minT", file);

  const Json& B = at(doc, "body", file);
  M.body.ms = real_at(B, "ms", file); M.body.KG = real_at(B, "KG", file);
  const std::vector<Real> g0 = numbers(at(B, "g0", file), file, "body.g0");
  same_size(file, "body.g0", g0.size(), 3);
  M.body.g0x = g0[0]; M.body.g0y = g0[1]; M.body.g0z = g0[2];
  M.body.XO = real_at(B, "XO", file); M.body.YO = real_at(B, "YO", file);
  M.body.mh0 = real_at(B, "mh0", file); M.body.Ith0 = real_at(B, "Ith0", file); M.body.Iph0 = real_at(B, "Iph0", file);
  M.body.cDz = real_at(B, "cDz", file); M.body.cDth = real_at(B, "cDth", file); M.body.cDph = real_at(B, "cDph", file);
  M.body.qz = real_at(B, "qz", file); M.body.qth = real_at(B, "qth", file); M.body.qph = real_at(B, "qph", file);

  const Json& R = at(doc, "readout", file);
  M.readout.xFP = real_at(R, "xFP", file);
  M.readout.xAP = real_at(R, "xAP", file);

  M.hashAll = verify_hashes(doc, file, {
    {"groups.nodeStart", 'i', M.gNodeStart.data(), ng}, {"groups.nodeCount", 'i', M.gNodeCount.data(), ng}, {"groups.nodes", 'i', M.gNodes.data(), M.gNodes.size()},
    {"groups.mode", 'b', M.gMode.data(), ng}, {"groups.px", 'f', M.gpx.data(), ng}, {"groups.py", 'f', M.gpy.data(), ng}, {"groups.pz", 'f', M.gpz.data(), ng},
    {"connections.a", 'i', M.ca.data(), ncn}, {"connections.b", 'i', M.cb.data(), ncn}, {"connections.type", 'b', M.cLaw.data(), ncn},
    {"connections.x", 'f', M.cx.data(), ncn}, {"connections.y", 'f', M.cy.data(), ncn}, {"connections.z", 'f', M.cz.data(), ncn},
    {"connections.area", 'f', M.cArea.data(), ncn}, {"connections.coef", 'f', M.cCoef.data(), ncn}, {"connections.kind", 'b', M.cKind.data(), ncn},
    {"connections.idx", 'i', M.cIdx.data(), ncn}, {"connections.en", 'b', M.cEn.data(), ncn}, {"connections.tOn", 'f', M.cTOn.data(), ncn},
    {"connections.skipGroup", 'i', M.cSkipGroup.data(), ncn}, {"connections.monitor", 'i', M.cMonitor.data(), ncn},
    {"marks.x", 'f', M.mkx.data(), M.mkx.size()}, {"marks.y", 'f', M.mky.data(), M.mky.size()}, {"marks.z", 'f', M.mkz.data(), M.mkz.size()},
    {"marks.when", 'b', M.mkWhen.data(), M.mkWhen.size()},
    {"state0.vol", 'f', M.vol0.data(), nn}, {"state0.level", 'f', M.level0.data(), nn},
    {"state0.cx", 'f', M.cx0.data(), nn}, {"state0.cy", 'f', M.cy0.data(), nn}, {"state0.cz", 'f', M.cz0.data(), nn},
  });
  return M;
}

void check_pairing(const CompiledShip& ship, const CompiledSim& sim) {
  if (sim.shipId != ship.id) throw std::runtime_error("simulation " + sim.id + " was compiled for ship " + sim.shipId + ", not " + ship.id);
  if (sim.shipHash != ship.hashAll) throw std::runtime_error("simulation " + sim.id + " was compiled against a different build of ship " + ship.id + " (hash " + sim.shipHash + " vs " + ship.hashAll + ")");
  if (sim.nodes() != ship.nodes()) throw std::runtime_error("simulation " + sim.id + " has " + std::to_string(sim.nodes()) + " nodes, ship " + ship.id + " has " + std::to_string(ship.nodes()));
}

}  // namespace sinksim::io
