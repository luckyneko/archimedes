# archimedes [![CI](https://github.com/luckyneko/archimedes/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/luckyneko/archimedes/actions/workflows/ci.yml)
C++17 Vulkan-based 2D/3D renderer

Archimedes is an early-stage renderer built as a static library. It currently targets a
small, explicit `acm::` API over Vulkan 1.3: swapchain rendering, offscreen
render-to-texture, texture upload/sampling, vertex/index/uniform/storage buffers,
descriptor sets, dynamic uniforms, depth, MSAA, mipmaps, compute pipelines, and a live
testbed. macOS runs through MoltenVK; the repo vendors the Vulkan runtime pieces needed
by runnable targets, so no system install is required for the normal build.

The detailed architecture and contributor notes live in [CLAUDE.md](CLAUDE.md).

## Build / Integrate

Archimedes builds out-of-source only.

### To build stand-alone

```sh
git clone https://github.com/luckyneko/archimedes.git
cd archimedes
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

The first configure fetches pinned dependencies into `.cache/fetch/` and extracts them
under the build directory. Tests that need a live Vulkan driver are SKIP-aware, so a
machine without a usable driver can still exercise the CPU-side and compile/API checks.

### To build only the library

```sh
cmake -S . -B build-lib \
  -DARCHIMEDES_BUILD_TESTBED=OFF \
  -DARCHIMEDES_BUILD_TESTING=OFF \
  -DARCHIMEDES_BUILD_BENCHMARK=OFF
cmake --build build-lib
```

### To add to your CMakeLists.txt

```cmake
add_subdirectory("path/to/archimedes")

add_executable(my-app main.cpp)
target_link_libraries(my-app PRIVATE archimedes)
```

`archimedes` itself links only `Vulkan::Headers`, because it is a static library. A
runnable executable that creates Vulkan objects must also link a Vulkan loader. The
repo exposes version-locked helper functions after `add_subdirectory`:

```cmake
add_subdirectory("path/to/archimedes")

acm_require_vulkan_runtime()

add_executable(my-app main.cpp)
target_link_libraries(my-app PRIVATE archimedes Vulkan::Loader)
acm_stage_vulkan_runtime(my-app)
```

On macOS, `acm_stage_vulkan_runtime(my-app)` stages the vendored MoltenVK ICD and emits a
launcher script that points the Vulkan loader at it. Elsewhere it is a no-op unless
there is runtime staging to do.

## Tested Platforms

Continuously built and tested on:

- Linux (Ubuntu 24.04, Release + Debug)
- macOS 15 (AppleClang, Release)
- Windows Server 2022 (MSVC, Release)

The hosted CI runs formatting, library-only builds, full builds, and `ctest`. Live
rendering validation still depends on the runner having a usable Vulkan driver; the GPU
tests skip when the driver or capability they need is unavailable.

## Features

- Static library with public headers free of raw Vulkan types, except the opt-in
  `acmVulkanInterop.h` seam.
- Stable typed resource handles for devices, swapchains, renderers, textures, buffers,
  pipelines, descriptors, samplers, and command buffers.
- Swapchain frame loop with resize/out-of-date recovery and per-frame-in-flight resource
  indices.
- Offscreen render targets for render-to-texture and readback tests.
- Texture upload, mip generation, trilinear sampling, anisotropic sampling, storage
  images, descriptor arrays, and alpha blending.
- Vertex/index buffers, uniform buffers, dynamic uniform buffers, storage buffers, depth,
  MSAA, wireframe/wide-line state, and configurable pipeline state.
- Compute pipelines and a renderer pre-pass for compute/barriers/transitions before the
  draw pass.
- Pooling Vulkan memory sub-allocator and deferred destruction keyed to completed queue
  submissions.
- Testbed examples that drive the API against live windows and real driver behavior.

## Testbed

The testbed is a runnable example framework. It builds by default when Archimedes is the
top-level project.

```sh
cmake -S . -B build
cmake --build build
./build/run_testbed.sh --list
./build/run_testbed.sh ripple-mesh
```

Available examples:

- `ripple-mesh`: two windows, shared compute-deformed mesh, one renderer thread per
  window.
- `instanced-cubes`: dynamic uniform buffer offsets over a cube grid.
- `compute-texture`: compute writes a storage image in the renderer pre-pass, then the
  draw pass samples it.
- `mirror`: render-to-texture, then sample the offscreen result.
- `sprite-atlas`: descriptor array plus alpha-blended 2D sprites.
- `wireframe`: polygon-line rendering and wide lines, feature-gated.

Run a short scripted smoke test with:

```sh
TESTBED_FRAME_CAP=120 ./build/run_testbed.sh compute-texture
```

## API Start

Include the umbrella header:

```cpp
#include <archimedes/archimedes.h>
```

Create the root objects explicitly. `Instance` and `Device` are unique owners; resources
created from them must be reset/destroyed before their owning root.

```cpp
acm::Instance instance("my-app", acm::Version{0, 1, 0});
if (!instance.valid())
	return 1;

acm::Surface surface = instance.createHeadlessSurface({800, 600});
std::vector<acm::SurfaceOption> options = instance.surfaceOptions(surface);
if (options.empty())
	return 1;

acm::Device device = instance.createDevice(options.front().device);
acm::SwapChain swapChain = device.createSwapChain(surface, options.front());
acm::Renderer renderer = device.createRenderer(swapChain);
```

A first draw is shaders plus a pipeline compatible with the render target. The renderer
opens dynamic rendering, sets viewport/scissor, and passes a command buffer to your draw
callback:

```cpp
std::vector<char> vertSpirv = loadSpirv("triangle.vert.spv");
std::vector<char> fragSpirv = loadSpirv("triangle.frag.spv");

acm::Shader vert = device.createShader(vertSpirv);
acm::Shader frag = device.createShader(fragSpirv);
acm::Pipeline pipeline = device.createPipeline({vert, frag}, swapChain.renderTarget(0));

renderer.render([&](acm::CommandBuffer& cmd, uint32_t frameIndex)
				{
	(void)frameIndex;
	cmd.bindPipeline(pipeline);
	cmd.draw(3); });
```

For texture sampling, upload CPU pixels once, bind the texture and sampler through a
descriptor set, and build the pipeline with that descriptor layout:

```cpp
acm::Texture texture = device.createTexture(acm::Format::B8G8R8A8_Unorm, {512, 512});
texture.upload(pixels.data(), pixels.size());

acm::Sampler sampler = device.createSampler();
acm::DescriptorSetLayout layout = device.createDescriptorSetLayout(1);
acm::DescriptorSet descriptors = device.createDescriptorSet(layout);
descriptors.setTexture(0, texture, sampler);

acm::PipelineConfig config;
config.descriptorLayout = layout;
acm::RenderTarget target = swapChain.renderTarget(0);
acm::Pipeline textured = device.createPipeline({fullscreenVert, sampleFrag}, target, config);
```

For compute that feeds the same frame, use the renderer pre-pass. Record compute,
barriers, and image transitions before dynamic rendering begins:

```cpp
renderer.render(
	[&](acm::CommandBuffer& cmd, uint32_t frameIndex)
	{
		(void)frameIndex;
		cmd.bindComputePipeline(computePipeline);
		cmd.bindComputeDescriptorSet(computePipeline, computeDescriptors);
		cmd.dispatch(groupsX, groupsY);
		cmd.bufferBarrier(storageBuffer, acm::ShaderStage::Compute, acm::ShaderStage::Vertex);
	},
	[&](acm::CommandBuffer& cmd, uint32_t frameIndex)
	{
		(void)frameIndex;
		cmd.bindPipeline(graphicsPipeline);
		cmd.draw(vertexCount);
	});
```

The testbed examples in [testbed/src/examples/](testbed/src/examples/) are the best
working references for complete setup code.

## Tests

The Catch2 suite covers handle semantics, resource pools, Vulkan instance/device/surface
plumbing, swapchains, renderer frames, offscreen rendering/readback, textures, samplers,
vertex/index/uniform/dynamic-uniform buffers, descriptors, depth, MSAA, wireframe,
compute, storage images, and multipass rendering.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Formatting is available through the CMake targets:

```sh
cmake --build build --target format-check
cmake --build build --target format
```

## Benchmarks

The benchmark executable exercises production API paths with Catch2 benchmarks.

```sh
cmake -S . -B build -DARCHIMEDES_BUILD_BENCHMARK=ON
cmake --build build
./build/run_bench-archimedes.sh "[fast]"
python3 bench/report.py "[fast]"
```

`bench/report.py` can write JSON, append to a history file, and compare against previous
runs. See [CLAUDE.md](CLAUDE.md#build--verify) for the detailed benchmark workflow.
