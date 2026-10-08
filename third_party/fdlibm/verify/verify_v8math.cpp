// verify_v8math.cpp
//
// v8math (fdlibm for the transcendental functions, V8's pow wrapper over the CRT) against the
// Node/V8 reference bit patterns written by gen_ref.js. Exit 0 when every function matches bit for bit.
#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <climits>
#include <cmath>
#include <string>
#include <vector>
#include <v8math/v8_math.hpp>

struct Stat { std::string name; long n = 0, mis = 0; long long maxulp = 0; int shown = 0; };

static double from_hex(const char* s) { uint64_t u = strtoull(s, nullptr, 16); double d; memcpy(&d, &u, 8); return d; }
static uint64_t bits(double d) { uint64_t u; memcpy(&u, &d, 8); return u; }
static int64_t ordinal(double d) {
    uint64_t u = bits(d);
    return (u & 0x8000000000000000ull) ? -(int64_t)(u & 0x7fffffffffffffffull) : (int64_t)u;
}
static long long ulp_distance(double a, double b) {
    if (std::isnan(a) && std::isnan(b)) return 0;
    if (std::isnan(a) || std::isnan(b)) return LLONG_MAX;
    int64_t d = ordinal(a) - ordinal(b);
    return d < 0 ? -d : d;
}
static bool eval(const std::string& fn, double x, double y, double& r) {
    if (fn == "pow")   { r = v8math::pow(x, y); return true; }
    if (fn == "sin")   { r = v8math::sin(x); return true; }
    if (fn == "cos")   { r = v8math::cos(x); return true; }
    if (fn == "tan")   { r = v8math::tan(x); return true; }
    if (fn == "asin")  { r = v8math::asin(x); return true; }
    if (fn == "exp")   { r = v8math::exp(x); return true; }
    if (fn == "log")   { r = v8math::log(x); return true; }
    if (fn == "atan2") { r = v8math::atan2(x, y); return true; }
    if (fn == "sqrt")  { r = v8math::sqrt(x); return true; }
    return false;
}

int main(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : "ref_node.txt";
    FILE* f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return 2; }
    std::vector<Stat> stats;
    for (const char* n : {"pow", "sin", "cos", "tan", "asin", "exp", "log", "atan2", "sqrt"}) { Stat s; s.name = n; stats.push_back(s); }
    char line[256], name[32], hx[32], hy[32], hr[32];
    long total = 0, unknown = 0, mis_total = 0;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "%31s %31s %31s %31s", name, hx, hy, hr) != 4) continue;
        total++;
        double x = from_hex(hx), y = from_hex(hy), ref = from_hex(hr), r;
        if (!eval(name, x, y, r)) { unknown++; continue; }
        Stat* s = nullptr;
        for (auto& st : stats) if (st.name == name) { s = &st; break; }
        s->n++;
        if (bits(r) != bits(ref) && !(std::isnan(r) && std::isnan(ref))) {
            long long u = ulp_distance(r, ref);
            s->mis++; mis_total++; if (u > s->maxulp) s->maxulp = u;
            if (s->shown < 5) { s->shown++; printf("  v8math mismatch %-5s x=%.17g y=%.17g node=%.17g v8math=%.17g (%lld ulp)\n", name, x, y, ref, r, u); }
        }
    }
    fclose(f);
    printf("\n%-6s %8s   %-26s\n", "fn", "samples", "v8math != node (max ulp)");
    for (const auto& s : stats) {
        if (s.n == 0) continue;
        printf("%-6s %8ld   %8ld  %6.2f%%  (%4lld)\n", s.name.c_str(), s.n, s.mis, 100.0 * s.mis / s.n, s.maxulp);
    }
    printf("\nlines %ld, unknown functions %ld, v8math mismatches %ld -> %s\n", total, unknown, mis_total,
           mis_total == 0 ? "v8math reproduces Node bit for bit on this set" : "v8math differs from Node");
    return mis_total == 0 ? 0 : 1;
}
