// v8_math.hpp
//
// Node 24 / V8 13.6 Math.* semantics for a C++ port that has to match a JavaScript oracle bit for bit.
// Header-only; link fdlibm::fdlibm (see ../fdlibm). MIT.
//
// Measured on this machine against 260,009 Node v24.16.0 samples (../fdlibm/verify/):
//   sin cos tan asin atan2 exp log : V8's base::ieee754 is a port of fdlibm, so fdlibm_* reproduces
//                                    Node exactly (0 mismatches in 20,000 samples per function).
//   pow : V8 13.6 (v8_flags.use_std_math_pow, default true) applies the ECMAScript special cases, folds
//         y == 2 to x*x and y == 0.5 to sqrt, and otherwise calls std::pow of the platform CRT.
//         The result therefore depends on the CRT the oracle ran on (UCRT here, glibc on Linux).
//         fdlibm's own pow, V8's legacy path, differs from the UCRT by one ulp in about 7.6% of inputs.
//   sqrt : correctly rounded by IEEE 754 everywhere, so std::sqrt.
//
// Compile the translation units that use this header with /fp:precise or /fp:strict (MSVC) or
// -ffp-contract=off (GCC/Clang). Never /fp:fast or -ffast-math: they may reroute std::pow to a
// vector library and contract a*b+c into an FMA, and the oracle does neither.
#pragma once
#include <cmath>
#include <limits>
#include <fdlibm_api.h>

namespace v8math {

// src/numbers/ieee754.cc, v8::internal::math::pow, V8 13.6.233.17 (Node 24.16.0)
inline double pow(double x, double y) {
    if (std::isnan(y)) return std::numeric_limits<double>::quiet_NaN();
    if (std::isinf(y) && (x == 1 || x == -1)) return std::numeric_limits<double>::quiet_NaN();
    if (std::isnan(x)) x = std::numeric_limits<double>::quiet_NaN();
    if (y == 2) return x * x;
    if (y == 0.5) {
        if (std::isinf(x)) return std::numeric_limits<double>::infinity();
        return std::sqrt(x + 0);   // +0 so that (-0) ** 0.5 is +0
    }
    return std::pow(x, y);
}

inline double sin(double x)   { return fdlibm_sin(x); }
inline double cos(double x)   { return fdlibm_cos(x); }
inline double tan(double x)   { return fdlibm_tan(x); }
inline double asin(double x)  { return fdlibm_asin(x); }
inline double atan2(double y, double x) { return fdlibm_atan2(y, x); }
inline double exp(double x)   { return fdlibm_exp(x); }
inline double log(double x)   { return fdlibm_log(x); }
inline double sqrt(double x)  { return std::sqrt(x); }

}  // namespace v8math
