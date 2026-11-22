# Tests, validation, testbed and benchmarks

The commands are in [CLAUDE.md](../CLAUDE.md#build-test-run). This file describes what each
harness covers and how it decides to pass, skip or fail.

## The test suite

[test/](../test/) builds one Catch2 binary, `test-archimedes`, gated by
`ARCHIMEDES_BUILD_TESTING` (on at top level); ctest discovers its test cases. The library
is a thin layer over `vk*`, so coverage splits in two:

- **Driver-free** (`[unit]`, `[handle]`, `[version]`, `[log]`, and the preset checks):
  handle and resource-pool semantics, the deferred destroy queue, `Version`, the log hook,
  `InstanceConfig`, `SurfacePreferences`, `DevicePreferences` and `PipelineConfig` presets.
- **Integration** (`[gpu]`): everything that needs a device. The rendering and compute
  tests render offscreen or dispatch, then read the result back, so they check what the GPU
  produced rather than that a call succeeded; the rest check plumbing a readback cannot
  show (heap choice, block sharing, frame-loop progress).

**Device kinds.** Every `[gpu]` test that picks its device through the shared helpers runs
once per device kind present: a Catch2 generator over the kinds the instance offers (best
hardware device, then the CPU device), so one run on a GPU machine proves both pathways. A
GPU-less machine runs each test once, on the CPU device, and reports it passed rather than
skipped. A `device: <name> (<kind>)` info line says which run failed. When the CPU driver is
staged, the binary is compiled with `ACM_EXPECT_CPU_DEVICE` and a missing CPU device (or no
instance at all) fails, because staging is broken. Without the CPU driver, a `[gpu]` test
skips when no driver, extension or capability is present, so a GPU-less build stays green.
Capability gaps (no 4x MSAA, no `fillModeNonSolid`, and so on) always skip.

**Helpers.** [vk_test_helpers.h](../test/vk_test_helpers.h) holds the shared scaffolding:
`requireInstance`, per-kind selection (`selectGraphicsOption`, `selectGraphicsDevice`,
`selectSurfaceOption`), `topRankedKind` for the benchmarks, the validation capture,
`ScopedLogHandler` (captures log messages for one scope and passes on what it declines, so
an unrelated validation error still fails the test), and `buildHeadlessStack()`, which
builds instance, headless surface, device and swapchain and skips with a specific reason at
any missing layer. It is backend-neutral: no raw Vulkan or platform headers. The one
exception in the suite is [main.cpp](../test/main.cpp), which asks the loader directly
whether the validation layer is installed. Reuse the helpers; do not re-roll them. Pipelines use SPIR-V embedded in
[test_spirv.h](../test/test_spirv.h) and [test_spirv_api_errors.h](../test/test_spirv_api_errors.h),
so the tests need no shader compiler; regenerate an entry from the GLSL in its comment with
`glslangValidator -V --target-env vulkan1.3 -x`.

**Swapchain-backed tests** (surface, swapchain, renderer, the two-thread renderer, the
stale-target recording test) use `Instance::createHeadlessSurface`. They skip for a device
that cannot present to that surface, which on Windows is every hardware device (see
[ROUGH-EDGES.md](ROUGH-EDGES.md)); the device-only tests (render-to-texture, compute,
descriptors, ...) run anywhere with a graphics queue.

**What the files cover.**

| Area | Files |
|---|---|
| Handles, pools, deferred destruction | `test_handles.cpp`, `test_resource_pool.cpp` (including a bounded concurrent stress test), `test_deferred_destroy_queue.cpp` |
| Instance, device, surface, swapchain | `test_instance.cpp`, `test_device.cpp` (feature config, invalid/moved/reset devices), `test_surface.cpp`, `test_swapchain.cpp` |
| CPU device | `test_cpu_device.cpp`: preference presets, the thread cap, direct loading with every loader-found driver hidden (what an elevated Windows process sees), `cpuDriver` off, ranking, and one compute shader giving identical results on a hardware device and the CPU device |
| Frame loop | `test_renderer.cpp` (frames, a recording error that is returned while rendering continues, a compute pre-pass), `test_renderer_threads.cpp` (two renderers on two threads through recreation) |
| Recording and pipelines | `test_recording_errors.cpp` (invalid, stale and other-device arguments, first error wins across copies, `submitSync` refusal, a retired target), `test_pipeline.cpp`, `test_pipeline_state.cpp` (presets, cull mode and front face), `test_wireframe.cpp`, `test_multipass.cpp` |
| Render targets | `test_texture.cpp` (render-to-texture readback), `test_depth.cpp` (rejection, compare ops, a non-depth pipeline on a depth target), `test_msaa.cpp` (resolved edges, per-sample shading) |
| Resources | `test_buffer.cpp` (heap by usage, write validation), `test_allocator.cpp` (block sharing, buffer/texture separation), `test_texture_upload.cpp` (upload and its validation), `test_mipmap.cpp` (generated top mip, levels limited to what upload can generate), `test_sampler.cpp`, `test_anisotropy.cpp` |
| Descriptors | `test_vertexbuffer.cpp`, `test_uniform.cpp`, `test_dynamic_uniform.cpp`, `test_descriptors.cpp` (storage buffer, multi-stage uniform, arrays, sampled image, write validation, retention after the caller drops its wrappers), `test_storage_image.cpp` |
| Compute | `test_compute.cpp` (a dispatch fills a storage buffer; a barrier carries a compute write into a fragment read in one command buffer) |
| Diagnostics | `test_diagnostics.cpp` (the log hook, the missing-layer warning, the validation harness), `test_fallback_warnings.cpp` (one warning per fallback: wireframe, line width, sample shading, anisotropy, mipmapped depth, clamped sample count) |

ctest reports a Catch2 SKIP as passed. Run the binary directly to see which assertions ran.

## Validation and sanitizers

[test/main.cpp](../test/main.cpp) sets `ACM_VALIDATION=1` when the loader lists
`VK_LAYER_KHRONOS_validation`, so every instance the suite creates, through the helpers or
not, runs validated with the debug messenger on. A value already in the environment wins,
so `ACM_VALIDATION=0` opts a run out. A Catch2 listener fails the test case during which a
validation error was logged, on any thread, at the end of each section, grouped by message
id and naming the device the helpers selected. A test that provokes an error on purpose
claims it with `acmtest::takeValidationErrors()`. Two `[!shouldfail]` self-tests in
`test_diagnostics.cpp` prove that a logged error does fail its test case.

`ARCHIMEDES_TEST_VALIDATION` chooses the policy: `AUTO` (validate when the layer is
installed, otherwise run as before), `REQUIRE` (a missing layer fails every `[gpu]` test
instead of letting the run test nothing) or `OFF` (instances keep their own configs).
`ARCHIMEDES_TEST_SYNC_VALIDATION` also turns on the layer's synchronization validation
(`VK_KHRONOS_VALIDATION_VALIDATE_SYNC`, which can be exported for one run instead).

Under `ARCHIMEDES_SANITIZE=thread` the binary reads [test/tsan.supp](../test/tsan.supp)
itself, so ctest, test discovery and direct runs all apply it without `TSAN_OPTIONS`. It
suppresses only frames in the uninstrumented SwiftShader library; do not add `acm::`
suppressions.

**Sandboxed macOS.** A sandboxed automation process may be denied Metal access on an Apple
Silicon host that has it. MoltenVK then reports `VK_ERROR_INCOMPATIBLE_DRIVER` ("Metal is
not available") and the hardware runs skip. Record that as a limitation of the execution
environment, not of the machine. The CPU device still runs there (headless rendering and
compute need no Metal), so the `[gpu]` tests execute on it; checking the hardware path and
GPU benchmark numbers needs a process with Metal access.

## Testbed

[testbed/](../testbed/) is a runnable example framework: a small runner plus swappable
examples, each exercising a slice of the renderer on a live driver and windows, the parts
headless tests cannot reach. `testbed <name>` runs one (default `ripple-mesh`), `--list`
prints them, and `--device gpu|cpu` ranks the shared device with
`DevicePreferences::Default()` or `Cpu()` (`cpu` fails unless the CPU device presents to
every window). `TESTBED_FRAME_CAP=<n>` runs n frames and exits cleanly, for scripted smoke
runs. Every example runs on the CPU device.

The framework, in `testbed/src/`:

- **`Example`** ([Example.h](../testbed/src/Example.h)), the plugin interface: `config()`
  (how many windows, and the swapchain's `depth` and `samples`), `onInit(device, views)`,
  `onUpdate(device, time)` (main thread, per frame), `onRenderView(viewIndex, time)` (record
  and present one window, possibly on a worker thread, so Vulkan only and no GLFW) and
  `onShutdown()`.
- **`RenderContext`** ([RenderContext.h](../testbed/src/RenderContext.h)), one window's
  plumbing: its `SwapChain` and `Renderer`, exposed as `renderer()`, `swapChain()`,
  `renderTarget()` and `extent()`.
- **`App`** ([App.cpp](../testbed/src/App.cpp)) owns the lifecycle: GLFW and the instance,
  the example's windows and surfaces (all GLFW on the main thread), one device that presents
  to every surface, a `RenderContext` per window, the loop (poll, `onUpdate`, render every
  view; fork-join across `RenderWorker`s for several windows, inline for one) and teardown
  with the device before the surfaces.
- **`RenderWorker`** ([RenderWorker.h](../testbed/src/RenderWorker.h)), one render thread per
  window for multi-window examples (`kick(time)`, `wait()`).
- **`ExampleRegistry`** ([ExampleRegistry.cpp](../testbed/src/ExampleRegistry.cpp)), the name
  to factory table; register new examples there. Shared helpers: `TbUtils.h`
  (`loadShader`), `TbMath.h` (perspective, lookAt, translate, rotate), `TbGeometry.h` (a
  cube).

The examples, in [testbed/src/examples/](../testbed/src/examples/):

| Name | Class | Exercises |
|---|---|---|
| `ripple-mesh` | `RippleMeshExample` | two windows on two render threads sharing one mesh (`Scene`) that `mesh.comp` deforms each frame in a `Storage` buffer, bracketed by `bufferBarrier`s and pulled through `gl_VertexIndex` in `mesh.vert` |
| `instanced-cubes` | `InstancedCubesExample` | a cube grid from one dynamic uniform buffer, with vertex and index buffers, depth and MSAA |
| `compute-texture` | `ComputeTextureExample` | a compute shader writing a storage image in the renderer's `prePass`, sampled fullscreen in the same command buffer, with a per-frame ring of its time uniform |
| `mirror` | `MirrorExample` | render-to-texture in the pre-pass, then sampled fullscreen |
| `sprite-atlas` | `SpriteAtlasExample` | translucent sprites from a 3-texture descriptor array with alpha blending, a triangle strip and dynamic uniforms |
| `wireframe` | `WireframeExample` | `PolygonMode::Line` and wide lines, which fall back (and the library logs a warning) when the features are off |

Examples use only the `acm::` API; the App's GLFW surface creation and adoption is the only
raw Vulkan. GLFW stays on the main thread, and swapchain recreation on a render thread
re-queries the Vulkan surface, not GLFW. Shaders live in
[testbed/shaders/](../testbed/shaders/), compile to `build/testbed/shaders/*.spv` through
`vk_target_shaders`, and load through `tb::loadShader` (the `TESTBED_SHADER_DIR` define in
[testbed/CMakeLists.txt](../testbed/CMakeLists.txt)). Add examples in place and reuse the
framework; do not fork a parallel harness.

## Benchmarks

`bench-archimedes` ([bench/](../bench/)) is a Catch2 benchmark binary, gated by
`ARCHIMEDES_BUILD_BENCHMARK`. Workloads must exercise production API paths, never copied
benchmark-only implementations: `command_recording` (public `CommandBuffer` draw
sequences), `descriptor_updates`, `resource_creation`, `renderer_frame` (headless
`Renderer::render` frames against real resources), and the driver-free `resource_pool` and
`resource_pool_collect`. Tags are `[bench][fast][gpu]` or `[bench][fast][cpu]`. GPU
workloads run on the top-ranked device only (`acmtest::KindPolicy::TopRanked`), so one run's
numbers belong to one device kind, and skip without a usable device. Resource-creation
workloads use bounded batches, so Catch2's calibration cannot pile up thousands of live
Vulkan objects before cleanup. Benchmarks run unvalidated unless `ACM_VALIDATION=1` is set.

[bench/report.py](../bench/report.py) runs the binary (`--exec <path>`, default
`./build/bench-archimedes`) and prints a compact summary, sorted by workload, section and
variant so discovery order does not churn the output:

- `--json-out <file>` writes one run record: UTC timestamp, git branch, hash and dirty
  state, the forwarded Catch2 arguments, the device (`bench-archimedes --device-info` prints
  the top-ranked device's name and `hardware` or `cpu` kind), and flat rows in nanoseconds.
- `--json-append <file>` maintains a `runs[]` history for graphing.
- `--compare <history>` compares against the latest earlier result for each workload,
  section and variant **from a run on the same device kind** (runs recorded before device
  kinds existed count as `hardware`), before any append. Its `SIGNAL` column divides the
  absolute difference of the two means by the `hypot` of the two standard deviations and
  classifies it as noise, weak, clear or strong; interactive terminals colour rows by
  direction, brighter for stronger signals.

[bench/history/](../bench/history/) keeps reference histories from named machines.
