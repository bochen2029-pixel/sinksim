// A deliberately tiny test harness: no dependencies, one executable per test file, non-zero exit on failure.
#pragma once

#include <cmath>
#include <cstdio>
#include <string>

namespace sinksim::test {

inline int g_failures = 0;
inline int g_checks = 0;

inline void report(const char* file, int line, const std::string& what) {
  ++g_failures;
  std::printf("  FAIL %s:%d  %s\n", file, line, what.c_str());
}

inline int finish(const char* name) {
  std::printf("%s: %d checks, %d failures\n", name, g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}

}  // namespace sinksim::test

#define CHECK(cond)                                                              \
  do {                                                                           \
    ++::sinksim::test::g_checks;                                                 \
    if (!(cond)) ::sinksim::test::report(__FILE__, __LINE__, #cond);             \
  } while (0)

#define CHECK_EQ(a, b)                                                                                             \
  do {                                                                                                             \
    ++::sinksim::test::g_checks;                                                                                   \
    if (!((a) == (b))) ::sinksim::test::report(__FILE__, __LINE__, std::string(#a " == " #b));                     \
  } while (0)

#define CHECK_NEAR(a, b, tol)                                                                                             \
  do {                                                                                                                    \
    ++::sinksim::test::g_checks;                                                                                          \
    const double _va = static_cast<double>(a), _vb = static_cast<double>(b);                                              \
    if (!(std::fabs(_va - _vb) <= (tol)))                                                                                 \
      ::sinksim::test::report(__FILE__, __LINE__, std::string(#a " ~ " #b) + " (" + std::to_string(_va) + " vs " + std::to_string(_vb) + ")"); \
  } while (0)

#define CHECK_THROWS(expr)                                                       \
  do {                                                                           \
    ++::sinksim::test::g_checks;                                                 \
    bool _thrown = false;                                                        \
    try { (void)(expr); } catch (...) { _thrown = true; }                        \
    if (!_thrown) ::sinksim::test::report(__FILE__, __LINE__, "no throw: " #expr); \
  } while (0)
