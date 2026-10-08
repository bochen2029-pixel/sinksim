// sinksim: build configuration, fundamental types and the host/device annotation.
// Everything under sinksim/kernel is written against these aliases so that the same source compiles as
// plain C++ (reference and WebAssembly) and as CUDA device code.
#pragma once

#include <cstddef>
#include <cstdint>

#if defined(__CUDACC__)
#define SS_HD __host__ __device__
#else
#define SS_HD
#endif

namespace sinksim {

using Real = double;        // state, accumulators and all reference arithmetic
using Index = std::int32_t; // array indices; -1 denotes "the sea" on a connection end
using Byte = std::uint8_t;  // small enums and flags stored in arrays

struct Version {
  int major, minor, patch;
};

inline constexpr Version kEngineVersion{0, 1, 0};
inline constexpr int kCompiledShipFormatVersion = 1;
inline constexpr int kCompiledSimFormatVersion = 1;
inline constexpr int kTraceFormatVersion = 1;

}  // namespace sinksim
