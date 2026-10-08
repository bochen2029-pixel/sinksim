#include "sinksim/io/json.hpp"

#include <charconv>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace sinksim::json {

namespace {

void dump_impl(const Json& j, std::string& out, int indent, int level) {
  switch (j.type()) {
    case Json::value_t::null: out += "null"; break;
    case Json::value_t::boolean: out += j.get<bool>() ? "true" : "false"; break;
    case Json::value_t::number_integer: out += std::to_string(j.get<long long>()); break;
    case Json::value_t::number_unsigned: out += std::to_string(j.get<unsigned long long>()); break;
    case Json::value_t::number_float: out += format_number(j.get<double>()); break;
    case Json::value_t::string: out += j.dump(); break;
    case Json::value_t::array: {
      out.push_back('[');
      bool first = true;
      for (const Json& x : j) {
        if (!first) out.push_back(',');
        first = false;
        dump_impl(x, out, -1, level + 1);
      }
      out.push_back(']');
      break;
    }
    case Json::value_t::object: {
      if (j.empty()) { out += "{}"; break; }
      out.push_back('{');
      const std::string pad = indent >= 0 ? "\n" + std::string(static_cast<std::size_t>((level + 1) * indent), ' ') : "";
      const std::string close = indent >= 0 ? "\n" + std::string(static_cast<std::size_t>(level * indent), ' ') : "";
      bool first = true;
      for (auto it = j.begin(); it != j.end(); ++it) {
        if (!first) out.push_back(',');
        first = false;
        out += pad;
        out += Json(it.key()).dump();
        out += indent >= 0 ? ": " : ":";
        dump_impl(it.value(), out, indent, level + 1);
      }
      out += close;
      out.push_back('}');
      break;
    }
    default: out += "null"; break;
  }
}

}  // namespace

std::string format_number(double d) {
  if (!std::isfinite(d)) return "null";
  if (d == 0) d = 0.0;
  char buf[64];
  const auto r = std::to_chars(buf, buf + sizeof buf, d);
  return std::string(buf, r.ptr);
}

std::string dump(const Json& j, int indent) {
  std::string out;
  dump_impl(j, out, indent, 0);
  return out;
}

Json read_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot open " + path);
  try {
    return Json::parse(in);
  } catch (const Json::parse_error& e) {
    throw std::runtime_error(path + ": " + e.what());
  }
}

void write_file(const std::string& path, const Json& j, int indent) {
  std::ofstream out(path, std::ios::binary);
  if (!out) throw std::runtime_error("cannot write " + path);
  out << dump(j, indent) << '\n';
}

const Json& member(const Json& obj, const char* key, const std::string& where) {
  if (!obj.is_object()) throw std::runtime_error(where + ": expected an object holding \"" + key + "\"");
  const auto it = obj.find(key);
  if (it == obj.end()) throw std::runtime_error(where + ": missing member \"" + key + "\"");
  return *it;
}

double number(const Json& obj, const char* key, const std::string& where) {
  const Json& v = member(obj, key, where);
  if (!v.is_number()) throw std::runtime_error(where + ": member \"" + key + "\" is not a number");
  return v.get<double>();
}

long long integer(const Json& obj, const char* key, const std::string& where) {
  const double d = number(obj, key, where);
  if (d != std::floor(d)) throw std::runtime_error(where + ": member \"" + key + "\" is not an integer");
  return static_cast<long long>(d);
}

bool boolean(const Json& obj, const char* key, const std::string& where) {
  const Json& v = member(obj, key, where);
  if (!v.is_boolean()) throw std::runtime_error(where + ": member \"" + key + "\" is not a boolean");
  return v.get<bool>();
}

std::string string(const Json& obj, const char* key, const std::string& where) {
  const Json& v = member(obj, key, where);
  if (!v.is_string()) throw std::runtime_error(where + ": member \"" + key + "\" is not a string");
  return v.get<std::string>();
}

std::string string_or(const Json& obj, const char* key, const std::string& fallback) {
  if (!obj.is_object()) return fallback;
  const auto it = obj.find(key);
  return (it != obj.end() && it->is_string()) ? it->get<std::string>() : fallback;
}

double number_or(const Json& obj, const char* key, double fallback) {
  if (!obj.is_object()) return fallback;
  const auto it = obj.find(key);
  return (it != obj.end() && it->is_number()) ? it->get<double>() : fallback;
}

}  // namespace sinksim::json
