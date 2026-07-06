# Unit + integration tests (Catch2). Device-touching tests ([gpu]) need the
# vendored Vulkan runtime (loader + MoltenVK ICD) and SKIP at runtime if no
# driver is present. enable_testing() + addcatch2 are done at the project root.

# Brings in Vulkan::Loader (to resolve archimedes' vk* symbols) and, on macOS,
# the ACM_MOLTENVK_ICD path used to point the loader at the vendored driver.
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

# Point the loader at the vendored MoltenVK ICD so [gpu] tests find a driver
# under ctest (no system install). Only set this when we actually vendor an ICD
# (Apple/MoltenVK) — elsewhere an empty VK_ICD_FILENAMES would tell the loader to
# enumerate *zero* ICDs, hiding any system-installed driver and forcing every
# [gpu] test to SKIP. Off-Apple we leave it unset so the system loader is used.
if(ACM_MOLTENVK_ICD)
    list(APPEND _acm_test_props PROPERTIES ENVIRONMENT "VK_ICD_FILENAMES=${ACM_MOLTENVK_ICD}")
endif()
catch_discover_tests(test-${PROJECT_NAME} ${_acm_test_props})
