// A faithful transliteration of netlib fdlibm 5.3 (sin, cos, pow, scalbn, copysign) as host-and-device functions:
// the same arithmetic, the same bit manipulation, the same constants, so that the CPU and the GPU compute the same
// bits on every platform. tests/test_portable_math.cpp holds it bit for bit against the vendored C build.
// Trigonometric arguments beyond 2^19 pi/2 would need fdlibm's big-argument reduction and fall back to the platform
// function; the kernel's angles are clamped two orders of magnitude below that bound.
// Sun's notice (third_party/fdlibm/LICENSE): permission to use, copy, modify, and distribute this software is freely
// granted, provided that this notice is preserved.
#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>

#include "sinksim/config.hpp"

namespace sinksim::fdlibm {

// ----------------------------------------------------------------------------- word access

SS_HD inline std::int32_t hi(double x) {
  std::uint64_t b;
  std::memcpy(&b, &x, 8);
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(b >> 32));
}
SS_HD inline std::uint32_t lo(double x) {
  std::uint64_t b;
  std::memcpy(&b, &x, 8);
  return static_cast<std::uint32_t>(b & 0xffffffffu);
}
SS_HD inline double make(std::int32_t h, std::uint32_t l) {
  const std::uint64_t b = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(h)) << 32) | l;
  double x;
  std::memcpy(&x, &b, 8);
  return x;
}
SS_HD inline double with_hi(double x, std::int32_t h) { return make(h, lo(x)); }
SS_HD inline double with_lo(double x, std::uint32_t l) { return make(hi(x), l); }
SS_HD inline std::int32_t shl(std::int32_t v, int s) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(v) << s); }

SS_HD inline double platform_sin(double x) {
#if defined(__CUDA_ARCH__)
  return ::sin(x);
#else
  return std::sin(x);
#endif
}
SS_HD inline double platform_cos(double x) {
#if defined(__CUDA_ARCH__)
  return ::cos(x);
#else
  return std::cos(x);
#endif
}
SS_HD inline double platform_sqrt(double x) {
#if defined(__CUDA_ARCH__)
  return ::sqrt(x);
#else
  return std::sqrt(x);
#endif
}
SS_HD inline double platform_fabs(double x) {
#if defined(__CUDA_ARCH__)
  return ::fabs(x);
#else
  return std::fabs(x);
#endif
}

// ----------------------------------------------------------------------------- s_copysign.c, s_scalbn.c

SS_HD inline double copysign(double x, double y) {
  return with_hi(x, static_cast<std::int32_t>((static_cast<std::uint32_t>(hi(x)) & 0x7fffffffu) | (static_cast<std::uint32_t>(hi(y)) & 0x80000000u)));
}

SS_HD inline double scalbn(double x, int n) {
  const double two54 = 1.80143985094819840000e+16, twom54 = 5.55111512312578270212e-17, huge = 1.0e+300, tiny = 1.0e-300;
  std::int32_t hx = hi(x);
  const std::uint32_t lx = lo(x);
  std::int32_t k = (hx & 0x7ff00000) >> 20;
  if (k == 0) {
    if ((lx | static_cast<std::uint32_t>(hx & 0x7fffffff)) == 0) return x;
    x *= two54;
    hx = hi(x);
    k = ((hx & 0x7ff00000) >> 20) - 54;
    if (n < -50000) return tiny * x;
  }
  if (k == 0x7ff) return x + x;
  k = k + n;
  if (k > 0x7fe) return huge * copysign(huge, x);
  if (k > 0) return with_hi(x, (hx & static_cast<std::int32_t>(0x800fffff)) | shl(k, 20));
  if (k <= -54) {
    if (n > 50000) return huge * copysign(huge, x);
    return tiny * copysign(tiny, x);
  }
  k += 54;
  x = with_hi(x, (hx & static_cast<std::int32_t>(0x800fffff)) | shl(k, 20));
  return x * twom54;
}

// ----------------------------------------------------------------------------- k_sin.c, k_cos.c

SS_HD inline double kernel_sin(double x, double y, int iy) {
  const double half = 5.00000000000000000000e-01, S1 = -1.66666666666666324348e-01, S2 = 8.33333333332248946124e-03,
               S3 = -1.98412698298579493134e-04, S4 = 2.75573137070700676789e-06, S5 = -2.50507602534068634195e-08,
               S6 = 1.58969099521155010221e-10;
  const std::int32_t ix = hi(x) & 0x7fffffff;
  if (ix < 0x3e400000) {
    if (static_cast<int>(x) == 0) return x;
  }
  const double z = x * x;
  const double v = z * x;
  const double r = S2 + z * (S3 + z * (S4 + z * (S5 + z * S6)));
  if (iy == 0) return x + v * (S1 + z * r);
  return x - ((z * (half * y - v * r) - y) - v * S1);
}

SS_HD inline double kernel_cos(double x, double y) {
  const double one = 1.00000000000000000000e+00, C1 = 4.16666666666666019037e-02, C2 = -1.38888888888741095749e-03,
               C3 = 2.48015872894767294178e-05, C4 = -2.75573143513906633035e-07, C5 = 2.08757232129817482790e-09,
               C6 = -1.13596475577881948265e-11;
  const std::int32_t ix = hi(x) & 0x7fffffff;
  if (ix < 0x3e400000) {
    if (static_cast<int>(x) == 0) return one;
  }
  const double z = x * x;
  const double r = z * (C1 + z * (C2 + z * (C3 + z * (C4 + z * (C5 + z * C6)))));
  if (ix < 0x3FD33333) return one - (0.5 * z - (z * r - x * y));
  double qx;
  if (ix > 0x3fe90000) qx = 0.28125;
  else qx = make(ix - 0x00200000, 0);
  const double hz = 0.5 * z - qx;
  const double a = one - qx;
  return a - (hz - (z * r - x * y));
}

// ----------------------------------------------------------------------------- e_rem_pio2.c (small and medium arguments)

// Returns the quadrant n and y0 + y1 = x - n pi/2 for |x| <= 2^19 pi/2; returns a negative count below -1000 when the
// argument is too large for this reduction (the caller then uses the platform function).
SS_HD inline int rem_pio2(double x, double& y0, double& y1) {
  const double half = 5.00000000000000000000e-01, invpio2 = 6.36619772367581382433e-01,
               pio2_1 = 1.57079632673412561417e+00, pio2_1t = 6.07710050650619224932e-11,
               pio2_2 = 6.07710050630396597660e-11, pio2_2t = 2.02226624879595063154e-21,
               pio2_3 = 2.02226624871116645580e-21, pio2_3t = 8.47842766036889956997e-32;
  const std::int32_t npio2_hw[32] = {
      0x3FF921FB, 0x400921FB, 0x4012D97C, 0x401921FB, 0x401F6A7A, 0x4022D97C, 0x4025FDBB, 0x402921FB,
      0x402C463A, 0x402F6A7A, 0x4031475C, 0x4032D97C, 0x40346B9C, 0x4035FDBB, 0x40378FDB, 0x403921FB,
      0x403AB41B, 0x403C463A, 0x403DD85A, 0x403F6A7A, 0x40407E4C, 0x4041475C, 0x4042106C, 0x4042D97C,
      0x4043A28C, 0x40446B9C, 0x404534AC, 0x4045FDBB, 0x4046C6CB, 0x40478FDB, 0x404858EB, 0x404921FB};
  const std::int32_t hx = hi(x);
  const std::int32_t ix = hx & 0x7fffffff;
  if (ix <= 0x3fe921fb) {
    y0 = x; y1 = 0;
    return 0;
  }
  if (ix < 0x4002d97c) {
    if (hx > 0) {
      double z = x - pio2_1;
      if (ix != 0x3ff921fb) {
        y0 = z - pio2_1t;
        y1 = (z - y0) - pio2_1t;
      } else {
        z -= pio2_2;
        y0 = z - pio2_2t;
        y1 = (z - y0) - pio2_2t;
      }
      return 1;
    }
    double z = x + pio2_1;
    if (ix != 0x3ff921fb) {
      y0 = z + pio2_1t;
      y1 = (z - y0) + pio2_1t;
    } else {
      z += pio2_2;
      y0 = z + pio2_2t;
      y1 = (z - y0) + pio2_2t;
    }
    return -1;
  }
  if (ix <= 0x413921fb) {
    const double t0 = platform_fabs(x);
    const int n = static_cast<int>(t0 * invpio2 + half);
    const double fn = static_cast<double>(n);
    double r = t0 - fn * pio2_1;
    double w = fn * pio2_1t;
    if (n < 32 && ix != npio2_hw[n - 1]) {
      y0 = r - w;
    } else {
      const std::int32_t j = ix >> 20;
      y0 = r - w;
      std::int32_t i = j - ((hi(y0) >> 20) & 0x7ff);
      if (i > 16) {
        double t = r;
        w = fn * pio2_2;
        r = t - w;
        w = fn * pio2_2t - ((t - r) - w);
        y0 = r - w;
        i = j - ((hi(y0) >> 20) & 0x7ff);
        if (i > 49) {
          t = r;
          w = fn * pio2_3;
          r = t - w;
          w = fn * pio2_3t - ((t - r) - w);
          y0 = r - w;
        }
      }
    }
    y1 = (r - y0) - w;
    if (hx < 0) {
      y0 = -y0; y1 = -y1;
      return -n;
    }
    return n;
  }
  return -1000000;
}

// ----------------------------------------------------------------------------- s_sin.c, s_cos.c

SS_HD inline double sin(double x) {
  const std::int32_t ix = hi(x) & 0x7fffffff;
  if (ix <= 0x3fe921fb) return kernel_sin(x, 0.0, 0);
  if (ix >= 0x7ff00000) return x - x;
  if (ix > 0x413921fb) return platform_sin(x);
  double y0, y1;
  const int n = rem_pio2(x, y0, y1);
  switch (n & 3) {
    case 0: return kernel_sin(y0, y1, 1);
    case 1: return kernel_cos(y0, y1);
    case 2: return -kernel_sin(y0, y1, 1);
    default: return -kernel_cos(y0, y1);
  }
}

SS_HD inline double cos(double x) {
  const std::int32_t ix = hi(x) & 0x7fffffff;
  if (ix <= 0x3fe921fb) return kernel_cos(x, 0.0);
  if (ix >= 0x7ff00000) return x - x;
  if (ix > 0x413921fb) return platform_cos(x);
  double y0, y1;
  const int n = rem_pio2(x, y0, y1);
  switch (n & 3) {
    case 0: return kernel_cos(y0, y1);
    case 1: return -kernel_sin(y0, y1, 1);
    case 2: return -kernel_cos(y0, y1);
    default: return kernel_sin(y0, y1, 1);
  }
}

// ----------------------------------------------------------------------------- e_pow.c

SS_HD inline double pow(double x, double y) {
  const double dp_h1 = 5.84962487220764160156e-01, dp_l1 = 1.35003920212974897128e-08;
  const double zero = 0.0, one = 1.0, two = 2.0, two53 = 9007199254740992.0, huge = 1.0e300, tiny = 1.0e-300;
  const double L1 = 5.99999999999994648725e-01, L2 = 4.28571428578550184252e-01, L3 = 3.33333329818377432918e-01,
               L4 = 2.72728123808534006489e-01, L5 = 2.30660745775561754067e-01, L6 = 2.06975017800338417784e-01;
  const double P1 = 1.66666666666666019037e-01, P2 = -2.77777777770155933842e-03, P3 = 6.61375632143793436117e-05,
               P4 = -1.65339022054652515390e-06, P5 = 4.13813679705723846039e-08;
  const double lg2 = 6.93147180559945286227e-01, lg2_h = 6.93147182464599609375e-01, lg2_l = -1.90465429995776804525e-09;
  const double ovt = 8.0085662595372944372e-0017;
  const double cp = 9.61796693925975554329e-01, cp_h = 9.61796700954437255859e-01, cp_l = -7.02846165095275826516e-09;
  const double ivln2 = 1.44269504088896338700e+00, ivln2_h = 1.44269502162933349609e+00, ivln2_l = 1.92596299112661746887e-08;

  double z, ax, z_h, z_l, p_h, p_l;
  double y1, t1, t2, r, s, t, u, v, w;
  std::int32_t i, j, k, yisint, n;
  const std::int32_t hx = hi(x), hy = hi(y);
  const std::uint32_t lx = lo(x), ly = lo(y);
  std::int32_t ix = hx & 0x7fffffff;
  const std::int32_t iy = hy & 0x7fffffff;

  if ((static_cast<std::uint32_t>(iy) | ly) == 0) return one;
  if (ix > 0x7ff00000 || (ix == 0x7ff00000 && lx != 0) || iy > 0x7ff00000 || (iy == 0x7ff00000 && ly != 0)) return x + y;

  yisint = 0;
  if (hx < 0) {
    if (iy >= 0x43400000) {
      yisint = 2;
    } else if (iy >= 0x3ff00000) {
      k = (iy >> 20) - 0x3ff;
      if (k > 20) {
        const std::uint32_t jj = ly >> (52 - k);
        if ((jj << (52 - k)) == ly) yisint = 2 - static_cast<std::int32_t>(jj & 1u);
      } else if (ly == 0) {
        j = iy >> (20 - k);
        if ((j << (20 - k)) == iy) yisint = 2 - (j & 1);
      }
    }
  }

  if (ly == 0) {
    if (iy == 0x7ff00000) {
      if (((static_cast<std::uint32_t>(ix - 0x3ff00000)) | lx) == 0) return y - y;
      if (ix >= 0x3ff00000) return (hy >= 0) ? y : zero;
      return (hy < 0) ? -y : zero;
    }
    if (iy == 0x3ff00000) {
      if (hy < 0) return one / x;
      return x;
    }
    if (hy == 0x40000000) return x * x;
    if (hy == 0x3fe00000) {
      if (hx >= 0) return platform_sqrt(x);
    }
  }

  ax = platform_fabs(x);
  if (lx == 0) {
    if (ix == 0x7ff00000 || ix == 0 || ix == 0x3ff00000) {
      z = ax;
      if (hy < 0) z = one / z;
      if (hx < 0) {
        if (((ix - 0x3ff00000) | yisint) == 0) {
          z = (z - z) / (z - z);
        } else if (yisint == 1) {
          z = -z;
        }
      }
      return z;
    }
  }

  n = (hx >> 31) + 1;
  if ((n | yisint) == 0) return (x - x) / (x - x);

  s = one;
  if ((n | (yisint - 1)) == 0) s = -one;

  if (iy > 0x41e00000) {
    if (iy > 0x43f00000) {
      if (ix <= 0x3fefffff) return (hy < 0) ? huge * huge : tiny * tiny;
      if (ix >= 0x3ff00000) return (hy > 0) ? huge * huge : tiny * tiny;
    }
    if (ix < 0x3fefffff) return (hy < 0) ? s * huge * huge : s * tiny * tiny;
    if (ix > 0x3ff00000) return (hy > 0) ? s * huge * huge : s * tiny * tiny;
    t = ax - one;
    w = (t * t) * (0.5 - t * (0.3333333333333333333333 - t * 0.25));
    u = ivln2_h * t;
    v = t * ivln2_l - w * ivln2;
    t1 = u + v;
    t1 = with_lo(t1, 0);
    t2 = v - (t1 - u);
  } else {
    double ss, s2, s_h, s_l, t_h, t_l;
    n = 0;
    if (ix < 0x00100000) {
      ax *= two53;
      n -= 53;
      ix = hi(ax);
    }
    n += ((ix) >> 20) - 0x3ff;
    j = ix & 0x000fffff;
    ix = j | 0x3ff00000;
    if (j <= 0x3988E) k = 0;
    else if (j < 0xBB67A) k = 1;
    else {
      k = 0;
      n += 1;
      ix -= 0x00100000;
    }
    ax = with_hi(ax, ix);
    const double bpk = k ? 1.5 : 1.0;
    const double dp_hk = k ? dp_h1 : 0.0;
    const double dp_lk = k ? dp_l1 : 0.0;

    u = ax - bpk;
    v = one / (ax + bpk);
    ss = u * v;
    s_h = ss;
    s_h = with_lo(s_h, 0);
    t_h = make(((ix >> 1) | 0x20000000) + 0x00080000 + shl(k, 18), 0);
    t_l = ax - (t_h - bpk);
    s_l = v * ((u - s_h * t_h) - s_h * t_l);
    s2 = ss * ss;
    r = s2 * s2 * (L1 + s2 * (L2 + s2 * (L3 + s2 * (L4 + s2 * (L5 + s2 * L6)))));
    r += s_l * (s_h + ss);
    s2 = s_h * s_h;
    t_h = 3.0 + s2 + r;
    t_h = with_lo(t_h, 0);
    t_l = r - ((t_h - 3.0) - s2);
    u = s_h * t_h;
    v = s_l * t_h + t_l * ss;
    p_h = u + v;
    p_h = with_lo(p_h, 0);
    p_l = v - (p_h - u);
    z_h = cp_h * p_h;
    z_l = cp_l * p_h + p_l * cp + dp_lk;
    t = static_cast<double>(n);
    t1 = (((z_h + z_l) + dp_hk) + t);
    t1 = with_lo(t1, 0);
    t2 = z_l - (((t1 - t) - dp_hk) - z_h);
  }

  y1 = y;
  y1 = with_lo(y1, 0);
  p_l = (y - y1) * t1 + y * t2;
  p_h = y1 * t1;
  z = p_l + p_h;
  j = hi(z);
  i = static_cast<std::int32_t>(lo(z));
  if (j >= 0x40900000) {
    if (((j - 0x40900000) | i) != 0) return s * huge * huge;
    if (p_l + ovt > z - p_h) return s * huge * huge;
  } else if ((j & 0x7fffffff) >= 0x4090cc00) {
    if (((static_cast<std::uint32_t>(j) - 0xc090cc00u) | static_cast<std::uint32_t>(i)) != 0) return s * tiny * tiny;
    if (p_l <= z - p_h) return s * tiny * tiny;
  }

  i = j & 0x7fffffff;
  k = (i >> 20) - 0x3ff;
  n = 0;
  if (i > 0x3fe00000) {
    n = j + (0x00100000 >> (k + 1));
    k = ((n & 0x7fffffff) >> 20) - 0x3ff;
    t = make(n & ~(0x000fffff >> k), 0);
    n = ((n & 0x000fffff) | 0x00100000) >> (20 - k);
    if (j < 0) n = -n;
    p_h -= t;
  }
  t = p_l + p_h;
  t = with_lo(t, 0);
  u = t * lg2_h;
  v = (p_l - (t - p_h)) * lg2 + t * lg2_l;
  z = u + v;
  w = v - (z - u);
  t = z * z;
  t1 = z - t * (P1 + t * (P2 + t * (P3 + t * (P4 + t * P5))));
  r = (z * t1) / (t1 - two) - (w + z * w);
  z = one - (r - z);
  j = hi(z);
  j += shl(n, 20);
  if ((j >> 20) <= 0) z = scalbn(z, n);
  else z = with_hi(z, hi(z) + shl(n, 20));
  return s * z;
}

}  // namespace sinksim::fdlibm
