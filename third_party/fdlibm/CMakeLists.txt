cmake_minimum_required(VERSION 3.16)
project(fdlibm VERSION 5.3 LANGUAGES C)

# netlib fdlibm 5.3 (Sun Microsystems, freely distributable), untouched in src/.
# Every public name is prefixed fdlibm_ by fdlibm_rename.h so the library links next to the
# platform CRT; consumers include fdlibm_api.h and link fdlibm::fdlibm.
# Built with _IEEE_LIBM (pure IEEE-754 results, no SVID/XOPEN error handling).

file(GLOB FDLIBM_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/e_*.c"
     "${CMAKE_CURRENT_SOURCE_DIR}/src/k_*.c"
     "${CMAKE_CURRENT_SOURCE_DIR}/src/s_*.c"
     "${CMAKE_CURRENT_SOURCE_DIR}/src/w_*.c")

add_library(fdlibm STATIC ${FDLIBM_SOURCES})
add_library(fdlibm::fdlibm ALIAS fdlibm)
set_target_properties(fdlibm PROPERTIES DEBUG_POSTFIX d C_STANDARD 99)
target_compile_definitions(fdlibm PRIVATE _IEEE_LIBM __LITTLE_ENDIAN)
target_include_directories(fdlibm PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>)

if(MSVC)
    # /fp:strict: no FMA contraction and no reassociation (the results must stay bit-exact).
    # /Zc:__STDC__: fdlibm.h only emits ANSI prototypes when __STDC__ is defined.
    target_compile_options(fdlibm PRIVATE
        "/FI${CMAKE_CURRENT_SOURCE_DIR}/fdlibm_rename.h" /Zc:__STDC__ /fp:strict /W1)
else()
    # The sources type-pun doubles through int pointers, so strict aliasing must be off;
    # no contraction, no builtin substitution, no fast-math.
    target_compile_options(fdlibm PRIVATE
        -include "${CMAKE_CURRENT_SOURCE_DIR}/fdlibm_rename.h"
        -fno-strict-aliasing -ffp-contract=off -fno-builtin -fno-fast-math -w)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
install(TARGETS fdlibm EXPORT fdlibmTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES fdlibm_api.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES LICENSE src/readme DESTINATION ${CMAKE_INSTALL_DATADIR}/fdlibm)
install(EXPORT fdlibmTargets NAMESPACE fdlibm:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fdlibm)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/fdlibmConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
configure_package_config_file(cmake/fdlibmConfig.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/fdlibmConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fdlibm)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/fdlibmConfig.cmake" "${CMAKE_CURRENT_BINARY_DIR}/fdlibmConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fdlibm)
