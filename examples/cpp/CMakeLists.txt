# Beginner demos. Included when SEMPER_BUILD_EXAMPLES=ON.
# Links the portable C++ core (semper_pipeline) — no JNI / C ABI required.

add_executable(run_translation run_translation.cpp)
target_link_libraries(run_translation PRIVATE semper_pipeline)
target_include_directories(run_translation PRIVATE ${SEMPER_PUBLIC_INCLUDE})
set_target_properties(run_translation PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples/cpp"
)
