/* verify_fdlibm.c
 *
 * Compares the prefixed fdlibm build and the platform CRT against Node/V8 reference values.
 * Usage: verify_fdlibm ref_node.txt
 * Reference lines (from gen_ref.js): <fn> <hex x> <hex y> <hex result>
 * Exit code 0 when fdlibm reproduces every Node result bit for bit, 1 otherwise.
 */
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include "../fdlibm_api.h"

typedef double (*fn1)(double);
typedef double (*fn2)(double, double);

struct entry {
    const char *name;
    int nargs;
    fn1 fd1; fn2 fd2;      /* fdlibm */
    fn1 crt1; fn2 crt2;    /* platform CRT */
    long n, fd_mis, crt_mis;
    long long fd_maxulp, crt_maxulp;
    int shown, shown_crt;
};

static double crt_pow(double x, double y) { return pow(x, y); }
static double crt_atan2(double y, double x) { return atan2(y, x); }
static double crt_sin(double x) { return sin(x); }
static double crt_cos(double x) { return cos(x); }
static double crt_tan(double x) { return tan(x); }
static double crt_asin(double x) { return asin(x); }
static double crt_exp(double x) { return exp(x); }
static double crt_log(double x) { return log(x); }
static double crt_sqrt(double x) { return sqrt(x); }

static struct entry FNS[] = {
    {"pow",   2, 0, fdlibm_pow,   0, crt_pow},
    {"sin",   1, fdlibm_sin,  0, crt_sin,  0},
    {"cos",   1, fdlibm_cos,  0, crt_cos,  0},
    {"tan",   1, fdlibm_tan,  0, crt_tan,  0},
    {"asin",  1, fdlibm_asin, 0, crt_asin, 0},
    {"exp",   1, fdlibm_exp,  0, crt_exp,  0},
    {"log",   1, fdlibm_log,  0, crt_log,  0},
    {"atan2", 2, 0, fdlibm_atan2, 0, crt_atan2},
    {"sqrt",  1, fdlibm_sqrt, 0, crt_sqrt, 0},
};
#define NFNS ((int)(sizeof(FNS) / sizeof(FNS[0])))

static double from_hex(const char *s) { uint64_t u = strtoull(s, NULL, 16); double d; memcpy(&d, &u, 8); return d; }
static uint64_t bits(double d) { uint64_t u; memcpy(&u, &d, 8); return u; }
/* ordinal on the number line so that an ulp distance is meaningful across zero */
static int64_t ordinal(double d) {
    uint64_t u = bits(d);
    return (u & 0x8000000000000000ull) ? -(int64_t)(u & 0x7fffffffffffffffull) : (int64_t)u;
}
static long long ulp_distance(double a, double b) {
    if (isnan(a) && isnan(b)) return 0;
    if (isnan(a) || isnan(b)) return LLONG_MAX;
    int64_t d = ordinal(a) - ordinal(b);
    return d < 0 ? -d : d;
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "ref_node.txt";
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return 2; }
    char line[256], name[32], hx[32], hy[32], hr[32];
    long total = 0, unknown = 0;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "%31s %31s %31s %31s", name, hx, hy, hr) != 4) continue;
        total++;
        struct entry *e = NULL;
        for (int i = 0; i < NFNS; i++) if (strcmp(FNS[i].name, name) == 0) { e = &FNS[i]; break; }
        if (!e) { unknown++; continue; }
        double x = from_hex(hx), y = from_hex(hy), ref = from_hex(hr);
        double fd = e->nargs == 2 ? e->fd2(x, y) : e->fd1(x);
        double crt = e->nargs == 2 ? e->crt2(x, y) : e->crt1(x);
        e->n++;
        if (bits(fd) != bits(ref) && !(isnan(fd) && isnan(ref))) {
            long long u = ulp_distance(fd, ref);
            e->fd_mis++; if (u > e->fd_maxulp) e->fd_maxulp = u;
            if (e->shown < 3) {
                e->shown++;
                printf("  fdlibm mismatch %-5s x=%.17g y=%.17g node=%.17g fdlibm=%.17g (%lld ulp)\n", name, x, y, ref, fd, u);
            }
        }
        if (bits(crt) != bits(ref) && !(isnan(crt) && isnan(ref))) {
            long long u = ulp_distance(crt, ref);
            e->crt_mis++; if (u > e->crt_maxulp) e->crt_maxulp = u;
            if (e->shown_crt < 8) {
                e->shown_crt++;
                printf("  CRT    mismatch %-5s x=%.17g y=%.17g node=%.17g crt=%.17g (%lld ulp)\n", name, x, y, ref, crt, u);
            }
        }
    }
    fclose(f);
    printf("\n%-6s %8s   %-28s %-28s\n", "fn", "samples", "fdlibm != node (max ulp)", "CRT != node (max ulp)");
    long fd_total = 0;
    for (int i = 0; i < NFNS; i++) {
        struct entry *e = &FNS[i];
        if (e->n == 0) continue;
        fd_total += e->fd_mis;
        printf("%-6s %8ld   %8ld  %6.2f%%  (%4lld)    %8ld  %6.2f%%  (%4lld)\n", e->name, e->n,
               e->fd_mis, 100.0 * e->fd_mis / e->n, e->fd_maxulp,
               e->crt_mis, 100.0 * e->crt_mis / e->n, e->crt_maxulp);
    }
    printf("\nlines %ld, unknown functions %ld, fdlibm mismatches %ld -> %s\n", total, unknown, fd_total,
           fd_total == 0 ? "fdlibm reproduces Node bit for bit on this set" : "fdlibm differs from Node");
    return fd_total == 0 ? 0 : 1;
}
