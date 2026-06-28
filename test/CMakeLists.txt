# Unit + integration tests (Catch2). Device-touching tests ([gpu]) need the
# vendored Vulkan runtime (loader + MoltenVK ICD) and SKIP at runtime if no
# driver is present. enable_testing() + addcatch2 are done at the project root.

# Brings in Vulkan::Loader (to resolve archimedes' vk* symbols) and, on macOS,
# the ACM_MOLTENVK_ICD path used to point the loader at the vendored driver.
acm_require_vulkan_runtime()

add_executable(test-${PROJECT_NAME}
    test_version.cpp
    test_handles.cpp
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
    test_uniform_ring.cpp
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
)

target_compile_features(test-${PROJECT_NAME} PRIVATE cxx_std_17)
target_link_libraries(test-${PROJECT_NAME} PRIVATE ${PROJECT_NAME} Vulkan::Loader Catch2::Catch2WithMain)

# Point the loader at the vendored MoltenVK ICD so [gpu] tests find a driver
# under ctest (no system install). Only set this when we actually vendor an ICD
# (Apple/MoltenVK) — elsewhere an empty VK_ICD_FILENAMES would tell the loader to
# enumerate *zero* ICDs, hiding any system-installed driver and forcing every
# [gpu] test to SKIP. Off-Apple we leave it unset so the system loader is used.
set(_acm_test_props TEST_PREFIX "(${PROJECT_NAME}) ")
if(ACM_MOLTENVK_ICD)
    list(APPEND _acm_test_props PROPERTIES ENVIRONMENT "VK_ICD_FILENAMES=${ACM_MOLTENVK_ICD}")
endif()
catch_discover_tests(test-${PROJECT_NAME} ${_acm_test_props})
