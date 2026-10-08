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

if(SINKSIM_ENABLE_CUDA)
  # The CUDA batch engine: the same kernel headers compiled as device code, one block per simulation.
  add_library(sinksim_cuda STATIC cuda/batch.cu)
  add_library(sinksim::cuda ALIAS sinksim_cuda)
  target_include_directories(sinksim_cuda PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cuda)
  target_link_libraries(sinksim_cuda PUBLIC sinksim::engine)
  target_compile_features(sinksim_cuda PUBLIC cxx_std_20 cuda_std_20)
  set_target_properties(sinksim_cuda PROPERTIES CUDA_SEPARABLE_COMPILATION OFF CUDA_RESOLVE_DEVICE_SYMBOLS ON)
  target_compile_definitions(sinksim_cuda PUBLIC SINKSIM_HAS_CUDA=1)
endif()
