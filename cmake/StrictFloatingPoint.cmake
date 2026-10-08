# Reference arithmetic must be reproducible across compilers and machines: no contraction of a*b+c into a
# fused multiply-add, no reassociation, no fast-math shortcuts. Every target that contains physics links this.
add_library(sinksim_strict_fp INTERFACE)
add_library(sinksim::strict_fp ALIAS sinksim_strict_fp)

if(MSVC)
  target_compile_options(sinksim_strict_fp INTERFACE
    /fp:strict /W4 /permissive- /EHsc /utf-8 /Zc:__cplusplus /Zc:preprocessor)
else()
  target_compile_options(sinksim_strict_fp INTERFACE
    -ffp-contract=off -fno-fast-math -Wall -Wextra)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(sinksim_strict_fp INTERFACE -fexcess-precision=standard)
  endif()
endif()
