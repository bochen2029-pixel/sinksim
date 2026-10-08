// Math policies for the kernel. Every transcendental call in the physics goes through a policy, chosen per
// simulation, so that the same binary can reproduce the JavaScript oracle, compute platform-independent bits
// shared with the GPU, or use the platform library:
//
//   OracleMath    host: the vendored fdlibm build for sin and cos, V8 13.6's pow semantics over the platform CRT
//                 (third_party/v8math); on Windows this reproduces Node 24 bit for bit. Device code has neither
//                 and uses PortableMath.
//   PortableMath  the host-and-device transliteration of fdlibm (kernel/fdlibm_portable.hpp): identical bits on
//                 every CPU and on the GPU; equal to OracleMath for sin and cos, within one ulp of it for pow.
//   StdMath       the platform <cmath> (CUDA device math on the device).
//
// ss_max and ss_min follow JavaScript's Math.max and Math.min for ordinary numbers: the larger (smaller) value,
// the first argument when equal.
#pragma once

#include <cmath>

#include "sinksim/config.hpp"
#include "sinksim/kernel/fdlibm_portable.hpp"

#if !defined(__CUDA_ARCH__)
#include <v8math/v8_math.hpp>
#endif

namespace sinksim {

SS_HD inline Real ss_sqrt(Real x) { return fdlibm::platform_sqrt(x); }
SS_HD inline Real ss_abs(Real x) { return fdlibm::platform_fabs(x); }
SS_HD inline Real ss_max(Real a, Real b) { return (b > a) ? b : a; }
SS_HD inline Real ss_min(Real a, Real b) { return (b < a) ? b : a; }

struct PortableMath {
  static constexpr const char* name = "portable";
  SS_HD static Real sin(Real x) { return fdlibm::sin(x); }
  SS_HD static Real cos(Real x) { return fdlibm::cos(x); }
  SS_HD static Real pow(Real x, Real y) { return fdlibm::pow(x, y); }
};

struct OracleMath {
  static constexpr const char* name = "oracle";
  SS_HD static Real sin(Real x) {
#if defined(__CUDA_ARCH__)
    return fdlibm::sin(x);
#else
    return v8math::sin(x);
#endif
  }
  SS_HD static Real cos(Real x) {
#if defined(__CUDA_ARCH__)
    return fdlibm::cos(x);
#else
    return v8math::cos(x);
#endif
  }
  SS_HD static Real pow(Real x, Real y) {
#if defined(__CUDA_ARCH__)
    return fdlibm::pow(x, y);
#else
    return v8math::pow(x, y);
#endif
  }
};

struct StdMath {
  static constexpr const char* name = "std";
  SS_HD static Real sin(Real x) { return fdlibm::platform_sin(x); }
  SS_HD static Real cos(Real x) { return fdlibm::platform_cos(x); }
  SS_HD static Real pow(Real x, Real y) {
#if defined(__CUDA_ARCH__)
    return ::pow(x, y);
#else
    return std::pow(x, y);
#endif
  }
};

}  // namespace sinksim
