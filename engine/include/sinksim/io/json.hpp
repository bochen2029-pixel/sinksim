// JSON for the data files: nlohmann::ordered_json (third_party/nlohmann) for parsing and the value model, plus a
// small writer that lays files out the way the JS tools do (objects one member per line, arrays always on one line)
// and prints doubles as the shortest round-trip decimal. Keys keep their insertion order.
#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace sinksim::json {

using Json = nlohmann::ordered_json;

// Throws std::runtime_error naming the file on a parse error.
Json read_file(const std::string& path);
void write_file(const std::string& path, const Json& j, int indent = 2);

// indent < 0: compact. Otherwise objects are one member per line and arrays stay on one line.
std::string dump(const Json& j, int indent = -1);

// Shortest decimal that round-trips the double; "null" for non-finite values; -0 prints as 0.
std::string format_number(double d);

// Typed member access with readable errors. `where` names the file or object for the message.
const Json& member(const Json& obj, const char* key, const std::string& where);
double number(const Json& obj, const char* key, const std::string& where);
long long integer(const Json& obj, const char* key, const std::string& where);
bool boolean(const Json& obj, const char* key, const std::string& where);
std::string string(const Json& obj, const char* key, const std::string& where);
std::string string_or(const Json& obj, const char* key, const std::string& fallback);
double number_or(const Json& obj, const char* key, double fallback);

}  // namespace sinksim::json
