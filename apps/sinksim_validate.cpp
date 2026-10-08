// sinksim_validate: run every case in a catalog with the oracle's run options and print the validation table;
// optionally compare with a reference table and write the engine's own.
//   sinksim_validate <catalog.json> [--compare validation.oracle.json] [--out validation.engine.json]
#include <chrono>
#include <cmath>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>

#include "cli.hpp"
#include "sinksim/io/compiled.hpp"
#include "sinksim/io/json.hpp"
#include "sinksim/simulation.hpp"

using namespace sinksim;
using json::Json;
namespace fs = std::filesystem;

int main(int argc, char** argv) {
  const cli::Args args = cli::Args::parse(argc, argv, {});
  if (args.positional.empty()) {
    std::fprintf(stderr, "usage: sinksim_validate <catalog.json> [--compare validation.oracle.json] [--out validation.engine.json]\n");
    return 2;
  }
  try {
    const fs::path catalogPath = args.positional[0];
    const std::string cat = catalogPath.string();
    const Json catalog = json::read_file(cat);
    if (json::string_or(catalog, "format", "") != "sinksim.catalog") throw std::runtime_error(cat + ": not a sinksim.catalog file");
    const fs::path dir = catalogPath.parent_path();
    const auto ship = io::load_ship((dir / json::string(catalog, "shipFile", cat)).string());
    Json reference;
    if (args.has("compare")) reference = json::read_file(args.get("compare", ""));
    const Json* refCases = reference.is_object() ? &json::member(reference, "cases", "reference table") : nullptr;

    Json outCases = Json::object();
    int mismatches = 0;
    const auto tStart = std::chrono::steady_clock::now();
    const Json& cases = json::member(catalog, "cases", cat);
    for (const Json& cs : cases) {
      const std::string id = json::string(cs, "id", cat);
      const std::string label = json::string(cs, "label", cat);
      const CompiledSim sim = io::load_sim((dir / json::string(cs, "file", cat)).string());
      io::check_pairing(*ship, sim);
      Simulation S(ship, sim);
      RunOptions opt;
      opt.tMax = json::number_or(cs, "tMaxH", 8) * 3600;
      opt.every = 30;
      opt.stopWhenStable = true;
      const RunResult r = S.run(opt);
      std::printf("%-44s %s  trim %.2f\xC2\xB0  list %.2f\xC2\xB0  water %.0f t\n", label.c_str(),
                  r.foundered ? ("FOUNDERS at " + cli::hm(r.founderT)).c_str() : ("afloat (checked to " + cli::hm(r.tEnd) + ")").c_str(),
                  r.final.trimDeg, r.final.listDeg, std::round(r.final.waterT));
      Json rec = Json::object();
      rec["label"] = label;
      rec["foundered"] = r.foundered;
      rec["founderT"] = r.foundered ? Json(r.founderT) : Json(nullptr);
      rec["founderMin"] = r.foundered ? Json(std::round(r.founderT / 60 * 10) / 10) : Json(nullptr);
      rec["endT"] = r.tEnd;
      rec["trimDeg"] = r.final.trimDeg;
      rec["listDeg"] = r.final.listDeg;
      rec["waterT"] = r.final.waterT;
      Json ev = Json::array();
      for (const Event& e : r.events) {
        Json x = Json::object();
        x["tSec"] = e.t;
        x["id"] = S.event_id(e);
        x["label"] = S.event_label(e);
        ev.push_back(std::move(x));
      }
      rec["events"] = std::move(ev);
      outCases[id] = rec;

      if (refCases) {
        const auto it = refCases->find(id);
        if (it == refCases->end()) { std::printf("   (no reference for %s)\n", id.c_str()); continue; }
        const Json& refCase = *it;
        const bool refFoundered = json::boolean(refCase, "foundered", id);
        std::string problems;
        if (refFoundered != r.foundered) problems += " verdict";
        if (refFoundered && r.foundered) {
          const double dT = std::fabs(json::number(refCase, "founderT", id) - r.founderT);
          if (dT > 1.0) problems += " founderT(" + std::to_string(dT) + " s)";
        }
        // Equilibria are tight; a foundering plunge is chaotic in its last minute and is compared loosely.
        const double tolTrim = refFoundered ? 0.5 : 0.01, tolWater = refFoundered ? 200 : 1, tolList = refFoundered ? 0.5 : 0.01;
        const double dTrim = std::fabs(json::number(refCase, "trimDeg", id) - r.final.trimDeg);
        const double dList = std::fabs(json::number(refCase, "listDeg", id) - r.final.listDeg);
        const double dWater = std::fabs(json::number(refCase, "waterT", id) - r.final.waterT);
        if (dTrim > tolTrim) problems += " trim(" + std::to_string(dTrim) + ")";
        if (dList > tolList) problems += " list(" + std::to_string(dList) + ")";
        if (dWater > tolWater) problems += " water(" + std::to_string(dWater) + " t)";
        if (!refFoundered) {
          const double dEnd = std::fabs(json::number(refCase, "endT", id) - r.tEnd);
          if (dEnd > 60) problems += " endT(" + std::to_string(dEnd) + " s)";
        }
        if (!problems.empty()) { ++mismatches; std::printf("   MISMATCH vs reference:%s\n", problems.c_str()); }
      }
    }
    const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - tStart).count();
    std::printf("%zu cases in %.1f s\n", cases.size(), s);
    if (args.has("out")) {
      Json doc = Json::object();
      doc["format"] = "sinksim.validation";
      doc["formatVersion"] = 1;
      doc["ship"] = ship->id;
      doc["producer"] = "sinksim_validate (C++ reference engine)";
      doc["cases"] = outCases;
      json::write_file(args.get("out", ""), doc, 2);
      std::printf("wrote %s\n", args.get("out", "").c_str());
    }
    if (refCases) std::printf("RESULT: %s (%d mismatching cases)\n", mismatches == 0 ? "PASS" : "FAIL", mismatches);
    return mismatches == 0 ? 0 : 1;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "sinksim_validate: %s\n", e.what());
    return 1;
  }
}
