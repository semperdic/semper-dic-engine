cmake_minimum_required(VERSION 3.22)
# Built via scikit-build-core, which points CMake at the engine root (native/)
# with -DSEMPER_BUILD_PYTHON=ON; native/CMakeLists.txt then add_subdirectory()s
# this file. The semper_pipeline target + SEMPER_PUBLIC_INCLUDE are already defined
# by the parent, so here we only add the extension module — no recursion into native.
find_package(pybind11 CONFIG REQUIRED)

pybind11_add_module(_semper src/module.cpp)
target_link_libraries(_semper PRIVATE semper_pipeline)

# Installed into the `semper` package dir; scikit-build-core packs it into the wheel.
install(TARGETS _semper LIBRARY DESTINATION semper)
