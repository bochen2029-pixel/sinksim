// Math primitives used by the kernel. Every transcendental call goes through here so that the implementation
// is a build-time choice and the physics never changes:
//
//   SINKSIM_MATH_V8 (CMake: -DSINKSIM_MATH=v8, the default)
//     sin and cos from the vendored fdlibm, pow with V8 13.6 semantics (third_party/v8math). On this machine
//     these reproduce Node 24's Math.* bit for bit (third_party/README.md), which is what lets the engine be
//     held to the JavaScript oracle exactly.
//   otherwise (-DSINKSIM_MATH=std)
//     the platform <cmath>.
//   CUDA device code
//     always the device math library: fdlibm and the CRT are host code. The GPU build is compared with the
//     CPU reference at tolerance (docs/DETERMINISM.md).
//
// ss_max and ss_min follow JavaScript's Math.max and Math.min for ordinary numbers: the larger (smaller)
// value, the first argument when equal.
#pragma once

#include <cmath>

#include "sinksim/config.hpp"

#if defined(SINKSIM_MATH_V8) && !defined(__CUDA_ARCH__)
#include <v8math/v8_math.hpp>
#define SINKSIM_MATH_V8_ACTIVE 1
#else
#define SINKSIM_MATH_V8_ACTIVE 0
#endif

namespace sinksim {

SS_HD inline Real ss_sqrt(Real x) { return std::sqrt(x); }

SS_HD inline Real ss_sin(Real x) {
#if SINKSIM_MATH_V8_ACTIVE
  return v8math::sin(x);
#else
  return std::sin(x);
#endif
}

SS_HD inline Real ss_cos(Real x) {
#if SINKSIM_MATH_V8_ACTIVE
  return v8math::cos(x);
#else
  return std::cos(x);
#endif
}

SS_HD inline Real ss_pow(Real x, Real y) {
#if SINKSIM_MATH_V8_ACTIVE
  return v8math::pow(x, y);
#else
  return std::pow(x, y);
#endif
}

SS_HD inline Real ss_abs(Real x) { return std::fabs(x); }
SS_HD inline Real ss_max(Real a, Real b) { return (b > a) ? b : a; }
SS_HD inline Real ss_min(Real a, Real b) { return (b < a) ? b : a; }

}  // namespace sinksim
