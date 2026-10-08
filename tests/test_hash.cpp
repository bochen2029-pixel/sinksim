#include "sinksim/hash.hpp"

#include <cstdint>
#include <string>

#include "support/check.hpp"

using namespace sinksim;

int main() {
  // Published FNV-1a 64-bit test vectors
  { Fnv1a64 h; CHECK_EQ(h.hex(), std::string("cbf29ce484222325")); }
  { Fnv1a64 h; h.update("a", 1); CHECK_EQ(h.hex(), std::string("af63dc4c8601ec8c")); }
  { Fnv1a64 h; h.update("foobar", 6); CHECK_EQ(h.hex(), std::string("85944171f73967e8")); }
  // A double is hashed as its 8 little-endian bytes: 1.0 = 00 00 00 00 00 00 f0 3f
  {
    const double one = 1.0;
    const unsigned char bytes[8] = {0, 0, 0, 0, 0, 0, 0xf0, 0x3f};
    Fnv1a64 a, b;
    a.update_f64(&one, 1);
    b.update(bytes, 8);
    CHECK_EQ(a.hex(), b.hex());
  }
  // Negative zero hashes like zero
  {
    const double z = 0.0, nz = -0.0;
    Fnv1a64 a, b;
    a.update_f64(&z, 1);
    b.update_f64(&nz, 1);
    CHECK_EQ(a.hex(), b.hex());
  }
  // int32 little-endian
  {
    const std::int32_t v = 0x01020304;
    const unsigned char bytes[4] = {4, 3, 2, 1};
    Fnv1a64 a, b;
    a.update_i32(&v, 1);
    b.update(bytes, 4);
    CHECK_EQ(a.hex(), b.hex());
  }
  return test::finish("test_hash");
}
