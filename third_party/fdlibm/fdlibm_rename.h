/* fdlibm_rename.h
 *
 * Force-included when compiling the untouched netlib fdlibm 5.3 sources in src/
 * (MSVC: cl /FI fdlibm_rename.h, GCC/Clang: -include fdlibm_rename.h).
 *
 * Every symbol fdlibm exports under its standard C name gets an fdlibm_ prefix, so the
 * library can be linked next to the platform CRT without duplicate-symbol clashes and so a
 * caller can choose per call site between the CRT's pow() and fdlibm_pow(). The internal
 * __ieee754_* and __kernel_* names are unique already and are left alone.
 *
 * Consumers must NOT include this file; they include fdlibm_api.h.
 */
#ifndef FDLIBM_RENAME_H
#define FDLIBM_RENAME_H

/* ANSI / POSIX */
#define acos        fdlibm_acos
#define asin        fdlibm_asin
#define atan        fdlibm_atan
#define atan2       fdlibm_atan2
#define cos         fdlibm_cos
#define sin         fdlibm_sin
#define tan         fdlibm_tan
#define cosh        fdlibm_cosh
#define sinh        fdlibm_sinh
#define tanh        fdlibm_tanh
#define exp         fdlibm_exp
#define frexp       fdlibm_frexp
#define ldexp       fdlibm_ldexp
#define log         fdlibm_log
#define log10       fdlibm_log10
#define modf        fdlibm_modf
#define pow         fdlibm_pow
#define sqrt        fdlibm_sqrt
#define ceil        fdlibm_ceil
#define fabs        fdlibm_fabs
#define floor       fdlibm_floor
#define fmod        fdlibm_fmod
#define erf         fdlibm_erf
#define erfc        fdlibm_erfc
#define gamma       fdlibm_gamma
#define hypot       fdlibm_hypot
#define isnan       fdlibm_isnan
#define finite      fdlibm_finite
#define j0          fdlibm_j0
#define j1          fdlibm_j1
#define jn          fdlibm_jn
#define y0          fdlibm_y0
#define y1          fdlibm_y1
#define yn          fdlibm_yn
#define lgamma      fdlibm_lgamma
#define acosh       fdlibm_acosh
#define asinh       fdlibm_asinh
#define atanh       fdlibm_atanh
#define cbrt        fdlibm_cbrt
#define logb        fdlibm_logb
#define nextafter   fdlibm_nextafter
#define remainder   fdlibm_remainder
#define scalb       fdlibm_scalb
#define matherr     fdlibm_matherr
#define significand fdlibm_significand
#define copysign    fdlibm_copysign
#define ilogb       fdlibm_ilogb
#define rint        fdlibm_rint
#define scalbn      fdlibm_scalbn
#define expm1       fdlibm_expm1
#define log1p       fdlibm_log1p
#define gamma_r     fdlibm_gamma_r
#define lgamma_r    fdlibm_lgamma_r
#define signgam     fdlibm_signgam

#endif /* FDLIBM_RENAME_H */
