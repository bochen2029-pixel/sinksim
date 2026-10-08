# Reference arithmetic must be reproducible across compilers and machines: no contraction of a*b+c into a
# fused multiply-add, no reassociation, no fast-math shortcuts. Every target that contains physics links this.
# The flags are per language so that CUDA targets (whose host passes go through the same compiler) share it.
add_library(sinksim_strict_fp INTERFACE)
add_library(sinksim::strict_fp ALIAS sinksim_strict_fp)

if(MSVC)
  target_compile_options(sinksim_strict_fp INTERFACE
    "$<$<COMPILE_LANGUAGE:CXX>:/fp:strict;/W4;/permissive-;/EHsc;/utf-8;/Zc:__cplusplus;/Zc:preprocessor>"
    "$<$<COMPILE_LANGUAGE:CUDA>:-fmad=false;--expt-relaxed-constexpr;-Xcompiler=/fp:strict;-Xcompiler=/EHsc;-Xcompiler=/utf-8>")
else()
  target_compile_options(sinksim_strict_fp INTERFACE
    "$<$<COMPILE_LANGUAGE:CXX>:-ffp-contract=off;-fno-fast-math;-Wall;-Wextra>"
    "$<$<COMPILE_LANGUAGE:CUDA>:-fmad=false;--expt-relaxed-constexpr;-Xcompiler=-ffp-contract=off;-Xcompiler=-fno-fast-math>")
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(sinksim_strict_fp INTERFACE "$<$<COMPILE_LANGUAGE:CXX>:-fexcess-precision=standard>")
  endif()
endif()
