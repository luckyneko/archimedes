# Archimedes testbed — a runnable harness that drives the acm:: API against a
# real window + swapchain. Windowing via GLFW; the Vulkan runtime (loader +
# MoltenVK ICD) is vendored, so this needs no system Vulkan install.

include(addGLFW)
acm_require_vulkan_runtime()
include(addGlslang)

add_executable(testbed
    src/main.cpp
    src/App.cpp
    src/RenderContext.cpp
    src/ExampleRegistry.cpp
    src/Scene.cpp
    src/examples/RippleMeshExample.cpp
    src/examples/InstancedCubesExample.cpp
    src/examples/ComputeTextureExample.cpp
    src/examples/MirrorExample.cpp
    src/examples/SpriteAtlasExample.cpp
    src/examples/WireframeExample.cpp
)

target_compile_features(testbed PRIVATE cxx_std_17)
target_include_directories(testbed PRIVATE src ${PROJECT_SOURCE_DIR}/private)
# archimedes is a static lib linking only Vulkan::Headers, so the executable
# must pull in the loader (Vulkan::Loader) to resolve its vk* symbols. Threads
# for the per-window render threads.
find_package(Threads REQUIRED)
target_link_libraries(testbed PRIVATE archimedes glfw Vulkan::Loader Threads::Threads)

# Compile shaders to <bindir>/shaders/*.spv and tell the app where to find them.
vk_target_shaders(testbed
    shaders/mesh.vert
    shaders/mesh.frag
    shaders/mesh.comp
    shaders/cube_instanced.vert
    shaders/cube_instanced.frag
    shaders/fullscreen.vert
    shaders/sample.frag
    shaders/plasma.comp
    shaders/sprite.vert
    shaders/sprite.frag
)
target_compile_definitions(testbed PRIVATE TESTBED_SHADER_DIR="${CMAKE_CURRENT_BINARY_DIR}/shaders")

# Stage the MoltenVK ICD next to the binary + emit build/run_testbed.sh.
acm_stage_vulkan_runtime(testbed)
