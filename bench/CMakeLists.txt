# Catch2 benchmarks. Device-touching workloads use the same vendored Vulkan
# runtime path as the tests/testbed and SKIP when a live driver is unavailable.

acm_require_vulkan_runtime()

add_executable(bench-${PROJECT_NAME}
    main.cpp
    command_recording.cpp
    descriptor_updates.cpp
    renderer_frame.cpp
    resource_pool.cpp
    resource_creation.cpp
)

target_compile_features(bench-${PROJECT_NAME} PRIVATE cxx_std_17)
target_include_directories(bench-${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/private ${PROJECT_SOURCE_DIR}/test)
target_link_libraries(bench-${PROJECT_NAME} PRIVATE ${PROJECT_NAME} Vulkan::Loader Catch2::Catch2)
set_target_properties(bench-${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

acm_stage_vulkan_runtime(bench-${PROJECT_NAME})
