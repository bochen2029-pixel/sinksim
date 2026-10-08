// FNV-1a 64-bit over the little-endian bytes of numeric arrays. The ship compiler and the engine hash the
// same arrays in the same order; equal hashes prove that a JSON round trip reproduced every value exactly.
// Negative zero is normalised to zero because JSON cannot represent the difference.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "sinksim/config.hpp"

namespace sinksim {

class Fnv1a64 {
 public:
  void update(const void* data, std::size_t n) {
    const auto* p = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < n; ++i) {
      h_ ^= static_cast<std::uint64_t>(p[i]);
      h_ *= 1099511628211ULL;
    }
  }
  void update_f64(const double* v, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
      double d = v[i];
      if (d == 0) d = 0.0;
      std::uint64_t bits;
      std::memcpy(&bits, &d, sizeof bits);
      unsigned char b[8];
      for (int k = 0; k < 8; ++k) b[k] = static_cast<unsigned char>((bits >> (8 * k)) & 0xffu);
      update(b, 8);
    }
  }
  void update_i32(const std::int32_t* v, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
      std::uint32_t bits;
      std::memcpy(&bits, &v[i], sizeof bits);
      unsigned char b[4];
      for (int k = 0; k < 4; ++k) b[k] = static_cast<unsigned char>((bits >> (8 * k)) & 0xffu);
      update(b, 4);
    }
  }
  void update_u8(const std::uint8_t* v, std::size_t n) { update(v, n); }

  std::uint64_t value() const { return h_; }
  std::string hex() const {
    static const char* digits = "0123456789abcdef";
    std::string s(16, '0');
    for (int i = 15; i >= 0; --i) s[static_cast<std::size_t>(i)] = digits[(h_ >> (4 * (15 - i))) & 0xfu];
    return s;
  }

 private:
  std::uint64_t h_ = 14695981039346656037ULL;
};

}  // namespace sinksim
