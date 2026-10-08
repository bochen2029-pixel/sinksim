cmake_minimum_required(VERSION 3.25)

project(sinksim
  VERSION 0.1.0
  DESCRIPTION "Deterministic time-domain ship flooding and sinking simulator"
  LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static C runtime on MSVC: the executables then carry their own pow, and the verification in third_party found the
# Node 24 agreement of the pow wrapper to hold with the static runtime (docs/DETERMINISM.md).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(SINKSIM_BUILD_APPS "Build the command-line tools" ON)
option(SINKSIM_BUILD_TESTS "Build and register the tests" ON)
option(SINKSIM_ENABLE_CUDA "Build the CUDA batch engine (needs the CUDA toolkit; computes the portable numerics)" OFF)

# Tolerances for the golden test; docs/DETERMINISM.md records the measured values behind them.
set(SINKSIM_GOLDEN_TOL_STEP "0" CACHE STRING "Largest allowed difference after one step from a golden record")
set(SINKSIM_GOLDEN_TOL_WINDOW "0" CACHE STRING "Largest allowed difference after one sampled window from a golden record")

if(SINKSIM_ENABLE_CUDA)
  enable_language(CUDA)
  set(CMAKE_CUDA_STANDARD 20)
  set(CMAKE_CUDA_STANDARD_REQUIRED ON)
  if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES OR CMAKE_CUDA_ARCHITECTURES STREQUAL "")
    set(CMAKE_CUDA_ARCHITECTURES 89 CACHE STRING "CUDA architectures" FORCE)
  endif()
endif()

include(cmake/StrictFloatingPoint.cmake)

add_subdirectory(third_party/fdlibm)
add_subdirectory(engine)
if(SINKSIM_BUILD_APPS)
  add_subdirectory(apps)
endif()
if(SINKSIM_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
