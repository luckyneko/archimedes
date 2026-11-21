# Unit + integration tests (Catch2). Device-touching tests ([gpu]) need the
# vendored Vulkan runtime (loader + staged drivers) and run once per device kind
# present (hardware, CPU); a missing kind SKIPs. enable_testing() + addcatch2 are done at the project root.

# Brings in Vulkan::Loader (to resolve archimedes' vk* symbols) and the staged drivers
# (MoltenVK on macOS, the CPU driver where available) the loader is pointed at.
acm_require_vulkan_runtime()

add_executable(test-${PROJECT_NAME}
    main.cpp
    test_version.cpp
    test_handles.cpp
    test_resource_pool.cpp
    test_deferred_destroy_queue.cpp
    test_instance.cpp
    test_device.cpp
    test_surface.cpp
    test_swapchain.cpp
    test_pipeline.cpp
    test_renderer.cpp
    test_texture.cpp
    test_sampler.cpp
    test_vertexbuffer.cpp
    test_uniform.cpp
    test_depth.cpp
    test_buffer.cpp
    test_texture_upload.cpp
    test_pipeline_state.cpp
    test_wireframe.cpp
    test_msaa.cpp
    test_anisotropy.cpp
    test_mipmap.cpp
    test_allocator.cpp
    test_descriptors.cpp
    test_compute.cpp
    test_dynamic_uniform.cpp
    test_storage_image.cpp
    test_multipass.cpp
    test_cpu_device.cpp
    test_diagnostics.cpp
)

target_compile_features(test-${PROJECT_NAME} PRIVATE cxx_std_17)
target_include_directories(test-${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/private)
target_link_libraries(test-${PROJECT_NAME} PRIVATE ${PROJECT_NAME} Vulkan::Loader Catch2::Catch2)
acm_stage_vulkan_runtime(test-${PROJECT_NAME})

# The tests link the vendored Vulkan loader, which on Windows is a DLL (vulkan-1.dll)
# built into its own directory rather than next to the test executable. On a machine
# with no system Vulkan runtime (e.g. a clean CI image) that DLL is otherwise
# unreachable, so the exe fails to even start (STATUS_DLL_NOT_FOUND / 0xc0000135) —
# breaking both Catch2's build-time test discovery and the ctest runs. DL_PATHS
# prepends the loader's directory to the DLL search path (PATH / LD_LIBRARY_PATH /
# DYLD_LIBRARY_PATH) for both. Locally it is harmless (the system loader is found first).
set(_acm_test_props TEST_PREFIX "(${PROJECT_NAME}) " DL_PATHS "$<TARGET_FILE_DIR:Vulkan::Loader>")

# Point the loader at the staged MoltenVK so [gpu] tests find it under ctest (no system
# install): VK_ICD_FILENAMES on macOS, nothing elsewhere — an empty VK_ICD_FILENAMES would
# hide every system driver. The CPU driver needs no environment: acm::Instance loads it
# directly from the test executable's vulkan/ folder.
acm_vulkan_driver_environment(_acm_driver_env)
if(_acm_driver_env)
    list(APPEND _acm_test_props PROPERTIES ENVIRONMENT "${_acm_driver_env}")
endif()

# With the CPU driver staged a CPU device must enumerate, so its absence fails the
# [gpu] tests (staging is broken) instead of letting them SKIP green.
if(ACM_SWIFTSHADER_ICD)
    target_compile_definitions(test-${PROJECT_NAME} PRIVATE ACM_EXPECT_CPU_DEVICE)
endif()

# Vulkan validation for the suite. test/main.cpp sets ACM_VALIDATION before any instance
# exists, so every instance turns on VK_LAYER_KHRONOS_validation, and a Catch2 listener
# fails the test case during which a validation error was logged.
#   AUTO    validate whenever the loader finds the layer; without it the suite runs as before
#   REQUIRE as AUTO, but a missing layer FAILs every [gpu] test instead of testing nothing
#   OFF     instances keep the validation their configs ask for
# ARCHIMEDES_TEST_SYNC_VALIDATION also turns on the layer's synchronization validation
# (VK_KHRONOS_VALIDATION_VALIDATE_SYNC, which can equally be exported for one run).
set(ARCHIMEDES_TEST_VALIDATION "AUTO" CACHE STRING "Vulkan validation in the tests: AUTO, REQUIRE or OFF")
set_property(CACHE ARCHIMEDES_TEST_VALIDATION PROPERTY STRINGS AUTO REQUIRE OFF)
option(ARCHIMEDES_TEST_SYNC_VALIDATION "Turn on the validation layer's synchronization validation in the tests" OFF)
string(TOUPPER "${ARCHIMEDES_TEST_VALIDATION}" _acm_test_validation)
if(_acm_test_validation STREQUAL "REQUIRE")
    target_compile_definitions(test-${PROJECT_NAME} PRIVATE ACM_REQUIRE_VALIDATION)
elseif(_acm_test_validation STREQUAL "OFF")
    target_compile_definitions(test-${PROJECT_NAME} PRIVATE ACM_TEST_VALIDATION_OFF)
elseif(NOT _acm_test_validation STREQUAL "AUTO")
    message(FATAL_ERROR "ARCHIMEDES_TEST_VALIDATION must be AUTO, REQUIRE or OFF, not '${ARCHIMEDES_TEST_VALIDATION}'")
endif()
if(ARCHIMEDES_TEST_SYNC_VALIDATION)
    target_compile_definitions(test-${PROJECT_NAME} PRIVATE ACM_TEST_SYNC_VALIDATION)
endif()

# ARCHIMEDES_SANITIZE instruments the suite like the library. Under ThreadSanitizer the
# binary reads test/tsan.supp itself (see main.cpp), so no TSAN_OPTIONS is needed.
acm_sanitize(test-${PROJECT_NAME})
acm_sanitizers(_acm_sanitizers)
if("thread" IN_LIST _acm_sanitizers)
    target_compile_definitions(test-${PROJECT_NAME} PRIVATE ACM_TSAN_SUPPRESSIONS="${CMAKE_CURRENT_SOURCE_DIR}/tsan.supp")
endif()

catch_discover_tests(test-${PROJECT_NAME} ${_acm_test_props})
