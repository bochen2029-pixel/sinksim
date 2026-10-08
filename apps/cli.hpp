// Minimal command-line parsing shared by the apps: positional arguments, --key value options, --flag switches.
#pragma once

#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "sinksim/simulation.hpp"

namespace sinksim::cli {

struct Args {
  std::vector<std::string> positional;
  std::map<std::string, std::string> options;
  std::set<std::string> flags;

  static Args parse(int argc, char** argv, const std::set<std::string>& flagNames) {
    Args a;
    for (int i = 1; i < argc; ++i) {
      std::string s = argv[i];
      if (s.size() > 2 && s[0] == '-' && s[1] == '-') {
        const std::string key = s.substr(2);
        if (flagNames.count(key)) {
          a.flags.insert(key);
        } else if (i + 1 < argc) {
          a.options[key] = argv[++i];
        } else {
          std::fprintf(stderr, "option --%s needs a value\n", key.c_str());
          std::exit(2);
        }
      } else {
        a.positional.push_back(s);
      }
    }
    return a;
  }

  bool has(const std::string& k) const { return options.count(k) != 0; }
  bool flag(const std::string& k) const { return flags.count(k) != 0; }
  std::string get(const std::string& k, const std::string& def) const {
    const auto it = options.find(k);
    return it == options.end() ? def : it->second;
  }
  double num(const std::string& k, double def) const {
    const auto it = options.find(k);
    return it == options.end() ? def : std::atof(it->second.c_str());
  }
};

// --numerics oracle|portable|std (default oracle); exits with usage on an unknown value.
inline sinksim::Numerics numerics_from(const Args& args) {
  sinksim::Numerics n = sinksim::Numerics::Oracle;
  const std::string s = args.get("numerics", "oracle");
  if (!sinksim::parse_numerics(s, n)) {
    std::fprintf(stderr, "unknown --numerics %s (use oracle, portable or std)\n", s.c_str());
    std::exit(2);
  }
  return n;
}

inline std::string minutes(double seconds) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.2f min", seconds / 60.0);
  return buf;
}

inline std::string hm(double seconds) {
  const int m = static_cast<int>(seconds / 60);
  char buf[32];
  std::snprintf(buf, sizeof buf, "%dh%02dm", m / 60, m % 60);
  return buf;
}

}  // namespace sinksim::cli
