# Unit + integration tests (Catch2). Device-touching tests ([gpu]) need the
# vendored Vulkan runtime (loader + MoltenVK ICD) and SKIP at runtime if no
# driver is present. enable_testing() + addcatch2 are done at the project root.

# Brings in Vulkan::Loader (to resolve archimedes' vk* symbols) and, on macOS,
# the ACM_MOLTENVK_ICD path used to point the loader at the vendored driver.
include(addVulkanRuntime)

add_executable(test-${PROJECT_NAME}
    test_version.cpp
    test_handles.cpp
    test_instance.cpp
    test_device.cpp
    test_surface.cpp
    test_swapchain.cpp
)

target_compile_features(test-${PROJECT_NAME} PRIVATE cxx_std_17)
target_link_libraries(test-${PROJECT_NAME} PRIVATE ${PROJECT_NAME} Vulkan::Loader Catch2::Catch2WithMain)

# Point the loader at the vendored MoltenVK ICD so [gpu] tests find a driver
# under ctest (no system install). Off-Apple this is empty and those tests SKIP.
catch_discover_tests(test-${PROJECT_NAME}
    TEST_PREFIX "(${PROJECT_NAME}) "
    PROPERTIES ENVIRONMENT "VK_ICD_FILENAMES=${ACM_MOLTENVK_ICD}"
)
