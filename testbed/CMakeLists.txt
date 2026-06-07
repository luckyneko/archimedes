# Archimedes testbed — a runnable harness that drives the acm:: API against a
# real window + swapchain. Windowing via GLFW; the Vulkan runtime (loader +
# MoltenVK ICD) is vendored, so this needs no system Vulkan install.

include(addGLFW)
include(addVulkanRuntime)

add_executable(testbed
    src/main.cpp
    src/Window.cpp
    src/MainDelegate.cpp
)

target_compile_features(testbed PRIVATE cxx_std_17)
target_include_directories(testbed PRIVATE src)
# archimedes is a static lib linking only Vulkan::Headers, so the executable
# must pull in the loader (Vulkan::Loader) to resolve its vk* symbols.
target_link_libraries(testbed PRIVATE archimedes glfw Vulkan::Loader)

# Stage the MoltenVK ICD next to the binary + emit build/run_testbed.sh.
acm_stage_vulkan_runtime(testbed)
