/* fdlibm_api.h
 *
 * Public interface of the prefixed fdlibm build (netlib fdlibm 5.3, Sun Microsystems,
 * "permission to use, copy, modify, and distribute this software is freely granted").
 *
 * Every function is the fdlibm implementation under an fdlibm_ prefix, compiled with
 * _IEEE_LIBM (pure IEEE-754 behaviour, no SVID/XOPEN error handling, no matherr calls).
 * V8's Math.pow is fdlibm e_pow.c, so fdlibm_pow reproduces Node's Math.pow bit for bit;
 * see verify/ for the measured agreement of every function against Node.
 *
 * This header deliberately does not include fdlibm.h, whose "struct exception" clashes
 * with the MSVC CRT; it can be included next to <math.h> or <cmath>.
 */
#ifndef FDLIBM_API_H
#define FDLIBM_API_H

#ifdef __cplusplus
extern "C" {
#endif

/* ANSI / POSIX */
double fdlibm_acos(double);
double fdlibm_asin(double);
double fdlibm_atan(double);
double fdlibm_atan2(double, double);
double fdlibm_cos(double);
double fdlibm_sin(double);
double fdlibm_tan(double);
double fdlibm_cosh(double);
double fdlibm_sinh(double);
double fdlibm_tanh(double);
double fdlibm_exp(double);
double fdlibm_frexp(double, int *);
double fdlibm_ldexp(double, int);
double fdlibm_log(double);
double fdlibm_log10(double);
double fdlibm_modf(double, double *);
double fdlibm_pow(double, double);
double fdlibm_sqrt(double);
double fdlibm_ceil(double);
double fdlibm_fabs(double);
double fdlibm_floor(double);
double fdlibm_fmod(double, double);
double fdlibm_erf(double);
double fdlibm_erfc(double);
double fdlibm_gamma(double);
double fdlibm_hypot(double, double);
int    fdlibm_isnan(double);
int    fdlibm_finite(double);
double fdlibm_j0(double);
double fdlibm_j1(double);
double fdlibm_jn(int, double);
double fdlibm_y0(double);
double fdlibm_y1(double);
double fdlibm_yn(int, double);
double fdlibm_lgamma(double);

/* BSD */
double fdlibm_acosh(double);
double fdlibm_asinh(double);
double fdlibm_atanh(double);
double fdlibm_cbrt(double);
double fdlibm_logb(double);
double fdlibm_nextafter(double, double);
double fdlibm_remainder(double, double);
double fdlibm_scalb(double, double);
double fdlibm_significand(double);

/* IEEE */
double fdlibm_copysign(double, double);
int    fdlibm_ilogb(double);
double fdlibm_rint(double);
double fdlibm_scalbn(double, int);
double fdlibm_expm1(double);
double fdlibm_log1p(double);
double fdlibm_gamma_r(double, int *);
double fdlibm_lgamma_r(double, int *);

extern int fdlibm_signgam;

#ifdef __cplusplus
}
#endif

#endif /* FDLIBM_API_H */
