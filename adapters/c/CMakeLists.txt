# Stable C ABI shared library. Included from native/CMakeLists.txt when
# SEMPER_BUILD_C_SDK=ON. Wraps the C++ core (semper_pipeline); no algorithm here.
add_library(semper_c SHARED semper_c.cpp)

target_link_libraries(semper_c PRIVATE semper_pipeline)

target_include_directories(semper_c PUBLIC
        $<BUILD_INTERFACE:${SEMPER_PUBLIC_INCLUDE}>
        $<INSTALL_INTERFACE:include>
)

set_target_properties(semper_c PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
)

if(NOT MSVC)
    target_compile_options(semper_c PRIVATE -O3 -fPIC -fvisibility=hidden)
    target_link_options(semper_c PRIVATE -fopenmp)
endif()

# ---------------------------------------------------------------------------
# Install + a find_package(Semper) config so downstreams can link Semper::semper_c
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS semper_c EXPORT SemperTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Only the C ABI header + the headers it includes are part of the shipped SDK.
install(FILES
        ${SEMPER_PUBLIC_INCLUDE}/semper/semper_c.h
        ${SEMPER_PUBLIC_INCLUDE}/semper/version.hpp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/semper)

install(EXPORT SemperTargets
        FILE SemperTargets.cmake
        NAMESPACE Semper::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Semper)

configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/SemperConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/SemperConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Semper)

write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/SemperConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/SemperConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/SemperConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Semper)

# C ABI smoke binary (optional; built whenever the C SDK is).
if(EXISTS "${SEMPER_ROOT}/tests/c/smoke.c")
    add_executable(semper_c_smoke ${SEMPER_ROOT}/tests/c/smoke.c)
    target_link_libraries(semper_c_smoke PRIVATE semper_c)
    target_include_directories(semper_c_smoke PRIVATE ${SEMPER_PUBLIC_INCLUDE})
    # smoke.c is C; link the C++ runtime pulled in by semper_c.
    set_target_properties(semper_c_smoke PROPERTIES LINKER_LANGUAGE CXX)
    if(NOT MSVC)
        target_link_options(semper_c_smoke PRIVATE -fopenmp)
    endif()
endif()
