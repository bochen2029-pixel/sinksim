# ADR 0004: Dependencies are vendored, never fetched

Status: accepted, 2026-10-08.

## Context

The engine must build from a checkout with a compiler and CMake, on Windows and Linux, for years, in CI and on
air-gapped machines, and must produce identical numbers everywhere. Package managers and fetch-at-configure steps
make the build depend on the network and on whatever version resolves that day.

## Decision

Everything the engine depends on is committed under `third_party/` with its license file and a line in
`third_party/README.md` giving version, origin, hash and purpose. Nothing is downloaded at build or test time. The
current set: netlib fdlibm 5.3 with prefixed symbols, the `v8math` header, nlohmann JSON 3.12.0 (single header), and
the V8 source files that document the math claims (reference only).

Tooling outside the engine (the JS exporter, the Python analysis scripts) uses only the language's standard library.

## Consequences

- Updating a dependency is a reviewed commit with the new hash in the README.
- The repository carries about two megabytes of third-party source; that is the price of reproducibility.
- A future test framework or GPU helper library goes through the same door or is written in-house (the test harness
  in `tests/support/check.hpp` is in-house for that reason).
