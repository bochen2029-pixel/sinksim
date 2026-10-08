# The engine: single-source kernel headers (engine/include/sinksim/kernel) plus the host-side model, IO and
# simulation driver. Dependencies are vendored under third_party (fdlibm for the oracle-exact math, nlohmann
# JSON for the data files).
add_library(sinksim_engine STATIC
  src/json.cpp
  src/model.cpp
  src/compiled.cpp
  src/trace.cpp
  src/simulation.cpp)
add_library(sinksim::engine ALIAS sinksim_engine)

target_include_directories(sinksim_engine PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party>)
target_link_libraries(sinksim_engine PUBLIC sinksim::strict_fp fdlibm::fdlibm)
target_compile_features(sinksim_engine PUBLIC cxx_std_20)

if(SINKSIM_MATH STREQUAL "v8")
  target_compile_definitions(sinksim_engine PUBLIC SINKSIM_MATH_V8=1)
elseif(NOT SINKSIM_MATH STREQUAL "std")
  message(FATAL_ERROR "SINKSIM_MATH must be v8 or std, got '${SINKSIM_MATH}'")
endif()
