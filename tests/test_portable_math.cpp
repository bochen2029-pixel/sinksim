// The host-and-device fdlibm port must reproduce the vendored C build bit for bit over the domains the kernel
// uses, and well beyond them.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <fdlibm_api.h>

#include "sinksim/kernel/fdlibm_portable.hpp"
#include "sinksim/kernel/math.hpp"
#include "support/check.hpp"

using namespace sinksim;

namespace {

// Deterministic 64-bit generator (SplitMix64) so the sample set is the same everywhere.
struct Rng {
  std::uint64_t s;
  std::uint64_t next() {
    std::uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
  }
  double uniform() { return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0); }
  double range(double lo, double hi) { return lo + (hi - lo) * uniform(); }
};

bool same_bits(double a, double b) {
  std::uint64_t x, y;
  std::memcpy(&x, &a, 8);
  std::memcpy(&y, &b, 8);
  return x == y || (std::isnan(a) && std::isnan(b));
}

}  // namespace

int main() {
  Rng rng{20261008ULL};
  int mismatchSin = 0, mismatchCos = 0, mismatchPow = 0, mismatchScalbn = 0;
  const int N = 400000;
  // sin and cos: the kernel's domain (clamped angles), the first reduction branches, and the whole medium range
  for (int i = 0; i < N; ++i) {
    double x;
    switch (i % 4) {
      case 0: x = rng.range(-1.5, 1.5); break;
      case 1: x = rng.range(-3.0, 3.0); break;
      case 2: x = rng.range(-100.0, 100.0); break;
      default: x = rng.range(-8.0e5, 8.0e5); break;
    }
    if (!same_bits(fdlibm::sin(x), fdlibm_sin(x))) ++mismatchSin;
    if (!same_bits(fdlibm::cos(x), fdlibm_cos(x))) ++mismatchCos;
  }
  // special values
  for (double x : {0.0, -0.0, 1e-30, -1e-30, 0.785398163397448279, 1.5707963267948966, 3.141592653589793, 1e-10, 1e300}) {
    if (!same_bits(fdlibm::sin(x), fdlibm_sin(x))) ++mismatchSin;
    if (!same_bits(fdlibm::cos(x), fdlibm_cos(x))) ++mismatchCos;
  }
  // pow: the weir law's exponents over (0, 1], general positive bases, integer and half exponents, negative bases
  for (int i = 0; i < N; ++i) {
    double x, y;
    switch (i % 6) {
      case 0: x = rng.uniform(); y = 1.5; break;
      case 1: x = rng.uniform(); y = 0.385; break;
      case 2: x = rng.range(1e-6, 1e3); y = rng.range(-3.0, 3.0); break;
      case 3: x = rng.range(0.5, 2.0); y = rng.range(-200.0, 200.0); break;
      case 4: x = -rng.range(0.1, 10.0); y = static_cast<double>(static_cast<int>(rng.range(-6.0, 6.0))); break;
      default: x = rng.range(1e-300, 1e300); y = rng.range(-1.0, 1.0); break;
    }
    if (!same_bits(fdlibm::pow(x, y), fdlibm_pow(x, y))) ++mismatchPow;
  }
  for (double x : {0.0, -0.0, 1.0, -1.0, 2.0, 0.5, 1e-310, 1e308, -3.0})
    for (double y : {0.0, 1.0, -1.0, 2.0, 0.5, 3.0, -2.0, 0.385, 1.5, 1e10, -1e10, 1024.0, -1075.0})
      if (!same_bits(fdlibm::pow(x, y), fdlibm_pow(x, y))) ++mismatchPow;
  for (int i = 0; i < 20000; ++i) {
    const double x = rng.range(-10.0, 10.0);
    const int n = static_cast<int>(rng.range(-1100.0, 1100.0));
    if (!same_bits(fdlibm::scalbn(x, n), fdlibm_scalbn(x, n))) ++mismatchScalbn;
  }
  std::printf("portable vs vendored fdlibm: sin %d, cos %d, pow %d, scalbn %d mismatches\n", mismatchSin, mismatchCos, mismatchPow, mismatchScalbn);
  CHECK_EQ(mismatchSin, 0);
  CHECK_EQ(mismatchCos, 0);
  CHECK_EQ(mismatchPow, 0);
  CHECK_EQ(mismatchScalbn, 0);

  // How far the platform CRT's pow (what the oracle calls) is from fdlibm's over the weir law's inputs: reported,
  // not asserted, because it is a property of the platform (docs/DETERMINISM.md).
  int crtDiffers = 0;
  const int M = 200000;
  for (int i = 0; i < M; ++i) {
    const double r = rng.uniform();
    const double y = (i & 1) ? 1.5 : 0.385;
    if (!same_bits(OracleMath::pow(r, y), PortableMath::pow(r, y))) ++crtDiffers;
  }
  std::printf("platform pow differs from fdlibm pow on %d of %d weir-law inputs (%.2f%%)\n", crtDiffers, M, 100.0 * crtDiffers / M);
  // The policies agree exactly on sin and cos
  int policyDiff = 0;
  for (int i = 0; i < 100000; ++i) {
    const double x = rng.range(-1.5, 1.5);
    if (!same_bits(OracleMath::sin(x), PortableMath::sin(x)) || !same_bits(OracleMath::cos(x), PortableMath::cos(x))) ++policyDiff;
  }
  CHECK_EQ(policyDiff, 0);
  return test::finish("test_portable_math");
}
