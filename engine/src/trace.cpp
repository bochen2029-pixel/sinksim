#include "sinksim/io/trace.hpp"

#include <stdexcept>
#include <string>

#include "sinksim/hash.hpp"
#include "sinksim/io/json.hpp"

namespace sinksim::io {

void TraceColumns::push(const State& st, const Outputs& out, const Scratch& sc, Index nodes) {
  n++;
  t.push_back(st.t); zO.push_back(st.zO); pitch.push_back(st.pitch); roll.push_back(st.roll);
  vz.push_back(st.vz); wth.push_back(st.wth); wph.push_back(st.wph);
  inflow.push_back(out.inflow); Vb.push_back(out.Vb); hullTop.push_back(out.hullTop);
  for (Index k = 0; k < nodes; ++k) vol.push_back(st.vol[k]);
  for (Index k = 0; k < nodes; ++k) level.push_back(st.level[k]);
  for (Index k = 0; k < nodes; ++k) cx.push_back(sc.cx[k]);
  for (Index k = 0; k < nodes; ++k) cy.push_back(sc.cy[k]);
  for (Index k = 0; k < nodes; ++k) cz.push_back(sc.cz[k]);
}

StateSnapshot TraceColumns::snapshot(Index i, Index nodes) const {
  const auto k = static_cast<std::size_t>(i);
  const auto off = k * static_cast<std::size_t>(nodes);
  return StateSnapshot{t[k], zO[k], pitch[k], roll[k], vz[k], wth[k], wph[k], &vol[off], &level[off], &cx[off], &cy[off], &cz[off]};
}

namespace {

using json::Json;

constexpr const char* kFields[] = {"t", "zO", "pitch", "roll", "vz", "wth", "wph", "inflow", "Vb", "hullTop", "vol", "level", "cx", "cy", "cz"};

template <class Columns>
auto& field(Columns& c, const std::string& f) {
  if (f == "t") return c.t;
  if (f == "zO") return c.zO;
  if (f == "pitch") return c.pitch;
  if (f == "roll") return c.roll;
  if (f == "vz") return c.vz;
  if (f == "wth") return c.wth;
  if (f == "wph") return c.wph;
  if (f == "inflow") return c.inflow;
  if (f == "Vb") return c.Vb;
  if (f == "hullTop") return c.hullTop;
  if (f == "vol") return c.vol;
  if (f == "level") return c.level;
  if (f == "cx") return c.cx;
  if (f == "cy") return c.cy;
  return c.cz;
}

bool per_node(const std::string& f) { return f == "vol" || f == "level" || f == "cx" || f == "cy" || f == "cz"; }

void load_columns(const Json& v, TraceColumns& c, Index nodes, const std::string& path, const std::string& name) {
  const std::string where = path + " " + name;
  c.n = static_cast<Index>(json::integer(v, "n", where));
  for (const char* fn : kFields) {
    const std::string f = fn;
    const Json& arr = json::member(v, fn, where);
    if (!arr.is_array()) throw std::runtime_error(path + ": " + name + "." + f + " must be an array");
    std::vector<Real>& dst = field(c, f);
    dst.clear();
    dst.reserve(arr.size());
    for (const Json& x : arr) {
      if (!x.is_number()) throw std::runtime_error(path + ": " + name + "." + f + " contains a non-number");
      dst.push_back(x.get<double>());
    }
    const std::size_t want = per_node(f) ? static_cast<std::size_t>(c.n) * static_cast<std::size_t>(nodes) : static_cast<std::size_t>(c.n);
    if (dst.size() != want)
      throw std::runtime_error(path + ": " + name + "." + f + " has " + std::to_string(dst.size()) + " values, expected " + std::to_string(want));
  }
}

Json columns_value(const TraceColumns& c, Fnv1a64& chain, Json& hash, const std::string& name) {
  Json v = Json::object();
  v["n"] = c.n;
  for (const char* fn : kFields) {
    const std::string f = fn;
    const std::vector<Real>& src = field(c, f);
    v[f] = src;
    Fnv1a64 one;
    one.update_f64(src.data(), src.size());
    chain.update_f64(src.data(), src.size());
    hash[name + "." + f] = one.hex();
  }
  return v;
}

void verify_columns(const Json& hash, const TraceColumns& c, Fnv1a64& chain, const std::string& name, const std::string& path) {
  for (const char* fn : kFields) {
    const std::string f = fn;
    const std::vector<Real>& src = field(c, f);
    Fnv1a64 one;
    one.update_f64(src.data(), src.size());
    chain.update_f64(src.data(), src.size());
    const auto it = hash.find(name + "." + f);
    if (it != hash.end() && it->is_string() && it->get<std::string>() != one.hex())
      throw std::runtime_error(path + ": hash mismatch on " + name + "." + f + ": the JSON round trip was not exact");
  }
}

}  // namespace

Trace load_trace(const std::string& path) {
  const Json doc = json::read_file(path);
  if (json::string_or(doc, "format", "") != "sinksim.trace") throw std::runtime_error(path + ": format is not sinksim.trace");
  if (json::integer(doc, "formatVersion", path) != kTraceFormatVersion) throw std::runtime_error(path + ": unsupported trace formatVersion");
  Trace tr;
  tr.role = json::string_or(doc, "role", "trace");
  tr.ship = json::string_or(doc, "ship", "");
  tr.sim = json::string_or(doc, "sim", "");
  if (doc.contains("meta")) {
    const Json& meta = doc["meta"];
    tr.title = json::string_or(meta, "title", "");
    tr.producer = json::string_or(meta, "producer", "");
    tr.engine = json::string_or(meta, "engine", "");
  }
  tr.dt = json::number(doc, "dt", path);
  tr.nodes = static_cast<Index>(json::integer(doc, "nodes", path));
  tr.denseSteps = static_cast<int>(json::integer(doc, "denseSteps", path));
  tr.sampleEvery = json::number(doc, "sampleEvery", path);
  tr.steps = json::integer(doc, "steps", path);
  tr.tEnd = json::number(doc, "tEnd", path);
  tr.foundered = json::boolean(doc, "foundered", path);
  tr.founderT = json::number(doc, "founderT", path);
  for (const Json& e : json::member(doc, "events", path))
    tr.events.push_back(TraceEvent{json::number(e, "t", path), json::string(e, "id", path), json::string(e, "label", path)});
  load_columns(json::member(doc, "dense", path), tr.dense, tr.nodes, path, "dense");
  load_columns(json::member(doc, "samples", path), tr.samples, tr.nodes, path, "samples");
  if (doc.contains("hash")) {
    const Json& hash = doc["hash"];
    Fnv1a64 chain;
    verify_columns(hash, tr.dense, chain, "dense", path);
    verify_columns(hash, tr.samples, chain, "samples", path);
    const auto all = hash.find("all");
    if (all != hash.end() && all->is_string() && all->get<std::string>() != chain.hex()) throw std::runtime_error(path + ": chained hash mismatch");
  }
  return tr;
}

void save_trace(const Trace& tr, const std::string& path) {
  Json doc = Json::object();
  doc["format"] = "sinksim.trace";
  doc["formatVersion"] = kTraceFormatVersion;
  doc["role"] = tr.role;
  doc["ship"] = tr.ship;
  doc["sim"] = tr.sim;
  Json meta = Json::object();
  meta["title"] = tr.title;
  meta["producer"] = tr.producer;
  meta["engine"] = tr.engine;
  doc["meta"] = meta;
  doc["dt"] = tr.dt;
  doc["nodes"] = tr.nodes;
  doc["denseSteps"] = tr.denseSteps;
  doc["sampleEvery"] = tr.sampleEvery;
  doc["steps"] = tr.steps;
  doc["tEnd"] = tr.tEnd;
  doc["foundered"] = tr.foundered;
  doc["founderT"] = tr.founderT;
  Json ev = Json::array();
  for (const TraceEvent& e : tr.events) {
    Json x = Json::object();
    x["t"] = e.t;
    x["id"] = e.id;
    x["label"] = e.label;
    ev.push_back(std::move(x));
  }
  doc["events"] = std::move(ev);
  Json hash = Json::object();
  Fnv1a64 chain;
  doc["dense"] = columns_value(tr.dense, chain, hash, "dense");
  doc["samples"] = columns_value(tr.samples, chain, hash, "samples");
  hash["all"] = chain.hex();
  doc["hash"] = hash;
  json::write_file(path, doc, 2);
}

}  // namespace sinksim::io
