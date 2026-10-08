# Unit tests (dependency-free harness in support/check.hpp) and the data-driven acceptance tests.
foreach(t test_hash test_json test_kernel test_portable_math)
  add_executable(${t} ${t}.cpp)
  target_link_libraries(${t} PRIVATE sinksim::engine)
  target_include_directories(${t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  set_target_properties(${t} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  add_test(NAME ${t} COMMAND ${t})
endforeach()

if(SINKSIM_BUILD_APPS)
  set(_ship ${CMAKE_SOURCE_DIR}/ships/titanic/titanic64.ship.json)
  # The engine must reproduce the oracle's golden trace window by window (docs/DETERMINISM.md).
  add_test(NAME golden_titanic64_titanic
    COMMAND sinksim_check ${_ship}
      ${CMAKE_SOURCE_DIR}/ships/titanic/sims/titanic.sim.json
      ${CMAKE_SOURCE_DIR}/data/golden/titanic64/titanic.trace.json
      --tol-step ${SINKSIM_GOLDEN_TOL_STEP} --tol-window ${SINKSIM_GOLDEN_TOL_WINDOW} --quiet)
  # The portable numerics (the bits every CPU and the GPU must produce) against their own reference, exactly.
  add_test(NAME golden_portable_titanic64_titanic
    COMMAND sinksim_check ${_ship}
      ${CMAKE_SOURCE_DIR}/ships/titanic/sims/titanic.sim.json
      ${CMAKE_SOURCE_DIR}/data/golden/titanic64/titanic.portable.trace.json
      --numerics portable --tol-step 0 --tol-window 0 --quiet)
  # The engine must reproduce the oracle's validation table (docs/VALIDATION.md).
  add_test(NAME validation_titanic64
    COMMAND sinksim_validate ${CMAKE_SOURCE_DIR}/ships/titanic/catalog.json
      --compare ${CMAKE_SOURCE_DIR}/data/validation/titanic64/validation.oracle.json
      --out ${CMAKE_BINARY_DIR}/validation.engine.json)
  if(SINKSIM_ENABLE_CUDA)
    # The GPU must produce the portable bits: its trace is checked step by step by the CPU in portable numerics.
    add_test(NAME cuda_trace_titanic64_titanic
      COMMAND sinksim_cuda_run ${_ship} ${CMAKE_SOURCE_DIR}/ships/titanic/sims/titanic.sim.json
        --count 8 --trace ${CMAKE_BINARY_DIR}/cuda_titanic.trace.json)
    set_tests_properties(cuda_trace_titanic64_titanic PROPERTIES FIXTURES_SETUP cuda_trace)
    add_test(NAME cuda_matches_portable
      COMMAND sinksim_check ${_ship} ${CMAKE_SOURCE_DIR}/ships/titanic/sims/titanic.sim.json
        ${CMAKE_BINARY_DIR}/cuda_titanic.trace.json --numerics portable --tol-step 0 --tol-window 0 --quiet)
    set_tests_properties(cuda_matches_portable PROPERTIES FIXTURES_REQUIRED cuda_trace)
  endif()
endif()
