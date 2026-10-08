# Third-party code

Everything the engine depends on is vendored here so that a checkout builds with nothing but a C/C++ compiler
and CMake. Nothing in this directory is modified except where a subdirectory's own README says so.

| Directory | What | Version and origin | License | Used for |
|---|---|---|---|---|
| `fdlibm/` | Sun's freely distributable libm, built with every public symbol prefixed `fdlibm_` | netlib fdlibm 5.3, `http://www.netlib.org/fdlibm/`, 84 files, unmodified in `fdlibm/src/` | Sun notice in every file and in `fdlibm/LICENSE`; the wrapper files are MIT | `sin` and `cos` in the kernel: V8's `Math.sin` and `Math.cos` are a port of fdlibm, so these reproduce the JavaScript oracle bit for bit |
| `v8math/v8_math.hpp` | Node 24 / V8 13.6 `Math.*` semantics in C++ | written for this project | MIT | `pow`: V8 13.6 folds `y == 2` to `x*x` and `y == 0.5` to `sqrt`, then calls the platform CRT's `std::pow`; this header does the same |
| `nlohmann/json.hpp` | JSON for Modern C++, single header | 3.12.0, `https://github.com/nlohmann/json/releases/tag/v3.12.0`, sha256 `aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63` | MIT (`nlohmann/LICENSE.MIT`) | reading and writing every data file |
| `v8-reference-node24.16/` | The V8 sources behind the claims above, exactly as Node 24.16.0 was built from them | `https://raw.githubusercontent.com/nodejs/node/v24.16.0/deps/v8/...` and Node's `common.gypi` | BSD-3-Clause (`LICENSE`) | reference only, not compiled |

## Why these and not the platform libm

The kernel must reproduce the JavaScript oracle step for step. Measured on this machine against 260,009 samples of
Node v24.16.0 (V8 13.6.233.17) over the input domains the flooding core uses (`fdlibm/verify/`):

| function | samples | fdlibm differs from Node | MSVC CRT differs from Node | `v8math` differs from Node |
|---|---|---|---|---|
| pow | 100,009 | 7,561 (1 ulp) | 24, all with y = 2 | 0 |
| sin | 20,000 | 0 | 553 | 0 |
| cos | 20,000 | 0 | 638 | 0 |
| tan | 20,000 | 0 | 965 | 0 |
| asin | 20,000 | 0 | 64 | 0 |
| sqrt | 20,000 | 0 | 0 | 0 |

`Math.pow` is the one function that depends on the C runtime the oracle ran on: V8 13.6 dispatches to the CRT
(`v8_flags.use_std_math_pow`, see `v8-reference-node24.16/flag-definitions.h` and `numbers/ieee754.cc`). On Windows that is
the UCRT, which the engine also links, so `std::pow` matches; on Linux glibc may round some inputs differently. That is also
why the golden trace shipped with the original package differs from a Node 24 regeneration on this machine by one ulp in one
hull coordinate (docs/DETERMINISM.md).

The engine exposes this as the `oracle` numerics (`engine/include/sinksim/kernel/math.hpp`). Device code cannot call
these host functions, so `engine/include/sinksim/kernel/fdlibm_portable.hpp` carries a host-and-device transliteration
of fdlibm's sin, cos, pow and scalbn (held bit for bit against the build here by `tests/test_portable_math.cpp`); it is
the `portable` numerics the GPU computes and the CPU can reproduce exactly (ADR 0006).

## Re-verifying after a Node or compiler change

`fdlibm/verify/gen_ref.js` writes Node's own results for the sample set, `verify_fdlibm.c` and `verify_v8math.cpp` compare
the C and C++ sides against them, and `fdlibm/build-msvc.cmd` runs the whole thing with plain `cl` (needs `node` on PATH and
Visual Studio 2022). Exit code 0 means `v8math` still matches Node bit for bit. The reference file it generates
(`verify/ref_node.txt`, 14 MB) is not kept in the repository.

Provenance: the downloads and their hashes were taken on 2026-10-08 by a parallel session of the same project; the
measurements above are its results, reproduced in `fdlibm/verify/result_*.txt`.
