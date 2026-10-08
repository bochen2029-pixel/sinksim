#include "sinksim/io/json.hpp"

#include <cmath>
#include <string>

#include "support/check.hpp"

using namespace sinksim;
using json::Json;

int main() {
  // numbers print as the shortest round-trip decimal, like JavaScript
  CHECK_EQ(json::format_number(4408.0), std::string("4408"));
  CHECK_EQ(json::format_number(0.5), std::string("0.5"));
  CHECK_EQ(json::format_number(1e21), std::string("1e+21"));
  CHECK_EQ(json::format_number(-0.0), std::string("0"));
  CHECK_EQ(json::format_number(0.1 + 0.2), std::string("0.30000000000000004"));
  // every double survives dump -> parse
  for (double d : {3.141592653589793, 1.0 / 3.0, 123456789.123456789, 5e-324, 1.7976931348623157e308, -2.2250738585072014e-308, 60.0, -7.0}) {
    const Json v = Json::parse(json::format_number(d));
    CHECK_EQ(v.get<double>(), d);
  }
  // parsing keeps member order and distinguishes integer from float storage without losing values
  const Json doc = Json::parse(" { \"b\": [1, 2.5, {\"c\": null}], \"a\": \"x\", \"n\": -7 } ");
  CHECK(doc.is_object());
  CHECK_EQ(doc.size(), static_cast<std::size_t>(3));
  CHECK_EQ(doc.begin().key(), std::string("b"));
  CHECK_EQ(doc.at("b").size(), static_cast<std::size_t>(3));
  CHECK_EQ(doc.at("b")[1].get<double>(), 2.5);
  CHECK(doc.at("b")[2].at("c").is_null());
  CHECK_EQ(json::string(doc, "a", "doc"), std::string("x"));
  CHECK_EQ(json::integer(doc, "n", "doc"), -7LL);
  CHECK_EQ(json::number_or(doc, "missing", 9.0), 9.0);
  CHECK_EQ(json::string_or(doc, "missing", "fb"), std::string("fb"));
  CHECK_THROWS(json::member(doc, "missing", "doc"));
  CHECK_THROWS(json::number(doc, "a", "doc"));
  CHECK_THROWS(json::integer(doc, "b", "doc"));
  CHECK_THROWS(json::boolean(doc, "n", "doc"));
  // the writer: objects one member per line, arrays always inline, compact when indent < 0
  CHECK_EQ(json::dump(doc), std::string("{\"b\":[1,2.5,{\"c\":null}],\"a\":\"x\",\"n\":-7}"));
  CHECK_EQ(json::dump(doc, 2), std::string("{\n  \"b\": [1,2.5,{\"c\":null}],\n  \"a\": \"x\",\n  \"n\": -7\n}"));
  CHECK_EQ(json::dump(Json::parse("{}"), 2), std::string("{}"));
  CHECK_EQ(json::dump(Json::parse("[]")), std::string("[]"));
  CHECK_EQ(json::dump(Json("q\"\\\n")), std::string("\"q\\\"\\\\\\n\""));
  CHECK_EQ(json::dump(Json(true)), std::string("true"));
  CHECK_EQ(json::dump(Json(60.0)), std::string("60"));
  CHECK_EQ(json::dump(Json(std::vector<double>{1.5, -0.0, 3})), std::string("[1.5,0,3]"));
  // building documents keeps insertion order
  Json built = Json::object();
  built["z"] = 1;
  built["a"] = 2;
  built["z"] = 3;
  CHECK_EQ(json::dump(built), std::string("{\"z\":3,\"a\":2}"));
  // malformed input is rejected
  CHECK_THROWS(Json::parse("[1,]"));
  CHECK_THROWS(Json::parse("{\"a\":1,}"));
  CHECK_THROWS(Json::parse("\"unterminated"));
  CHECK_THROWS(Json::parse("[1] x"));
  CHECK_THROWS(Json::parse("{a:1}"));
  CHECK_THROWS(json::read_file("this/file/does/not/exist.json"));
  return test::finish("test_json");
}
