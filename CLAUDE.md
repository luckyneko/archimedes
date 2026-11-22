# Archimedes

A Vulkan 1.3 2D/3D renderer in C++17: a static library (`archimedes`), a Catch2 test suite,
Catch2 benchmarks (`bench-archimedes`) and a GLFW testbed of live examples. This file holds
what a coding agent needs in every session: the invariants, the conventions and the
commands. The mechanisms behind them are in [docs/](docs/). The working discipline in
[AGENTS.md](AGENTS.md) applies everywhere; this file overrides it where more specific.

## Status

Early-stage, verified end to end on MoltenVK (Apple Silicon), a Windows hardware driver
and the **CPU device**: SwiftShader, staged beside every runnable target so rendering and
compute run without a GPU driver or on the CPU by choice
([ADR-0001](docs/adr/0001-cpu-device-via-staged-swiftshader.md)). It has a swapchain frame
loop that survives resizes and runs on several threads, offscreen render-to-texture,
textures (upload, mipmaps, storage), buffers, descriptor sets (uniform, dynamic uniform,
storage, sampled images, arrays), depth, MSAA, configurable pipeline state, compute, a
pooling memory allocator, error-returning recording and descriptor calls, and a log hook
with Vulkan validation in the tests and CI.

## Where to read more

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): layers, handles and pools, ownership graph,
  frame loop, threading, recording errors, resources, memory, render targets, pipelines,
  descriptors, compute, diagnostics.
- [docs/TOOLING.md](docs/TOOLING.md): dependencies and vendoring, the staged Vulkan runtime,
  every CMake option, CI lanes.
- [docs/TESTING.md](docs/TESTING.md): the test suite, validation and sanitizers, the testbed,
  benchmarks and `bench/report.py`.
- [docs/ROUGH-EDGES.md](docs/ROUGH-EDGES.md): known limitations. [WORK.md](WORK.md): parked
  work in priority order. [CONTEXT.md](CONTEXT.md): vocabulary. [docs/adr/](docs/adr/):
  decisions. [README.md](README.md): the public front door.

## Invariants

Do not break these. Each is explained in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

**Handles and the backend**

- A public resource wrapper holds one `ResourceRef<backend::T>`: a stable slot pointer plus
  the slot ID it was issued for. No per-resource pImpl, no virtuals, no `operator->`.
  `access()` returns `nullptr` for a stale or invalid ref, and wrapper code branches on it
  before calling the backend.
- A slot's lifetime state is one atomic word (generation and count): `valid()` and
  `access()` are lock-free, retain and release are compare-exchanges, retirement pushes the
  slot onto its pool's lock-free retired list. Keep locks off that path. Pool allocation
  and the free list are under the pool mutex.
- A dead slot is reused only after its pool's sweep has handed the payload to the
  collection policy. Device pools hand payloads to the deferred destroy queue, which
  destroys them once the submission serial they were tagged with completes.
- Backend selection is compile-time: [acmBackend.h](include/archimedes/acmBackend.h)
  aliases `acm::backend::T` to `acm::vulkan::T`. No virtual dispatch, type erasure, casts or
  runtime renderer switching.
- No Vulkan types in mainline public headers. [acmVulkanInterop.h](include/archimedes/acmVulkanInterop.h)
  (`acm::interop`) is the only public header that includes Vulkan.
- `backend()` is an internal seam, not a native-handle API; slot IDs never leave
  `ResourceRef` and `ResourceSlot`. Backend classes collaborate through narrow, validated
  operations on the typed slot they already hold, never by rediscovering a resource from an
  index.
- Public wrapper methods live in the matching `src/acm*.cpp`, which reaches the backend only
  through [backendAPI.h](private/archimedes/backendAPI.h). A private resource's
  construction, native state, operations, retention and destruction live in
  `private/archimedes/vulkan/<Type>.h` and `src/vulkan/<Type>.cpp`.
- Apart from the `acm::Instance` root itself, factories are the only public construction
  path (`Instance::create*`, `Device::create*`, `CommandPool::allocate`,
  `interop::adoptSurface`). Friendship is limited to the approved private constructors.

**Ownership and teardown**

- `acm::Instance` and `acm::Device` are unique, move-only owning roots, each holding its
  heap-stable backend in a `unique_ptr`. There is no shared ownership.
- The `Instance` outlives every `Device` and `Surface`; a `Device` outlives every resource
  made from it. Reset children before their root. An adopted surface's native window must
  outlive the `acm::Surface`.
- A resource that needs another keeps it alive by retaining its public wrapper (a
  `RenderTarget` its texture, a `DescriptorSet` what was written into it). Command buffers
  do **not** retain what they record: keep recorded resources alive until `render()` or
  `submitSync()` returns.

**Threading**

- Several `Renderer`s may share a `Device`, each over its own `SwapChain` on its own thread.
  Recording is unlocked; the device's one queue mutex serializes submit, present and the
  queue and device waits.
- Distinct wrapper copies may be retained, released and read concurrently. Mutating or
  resetting one wrapper object, and mutating one native resource (writing a buffer or a
  descriptor set, recreating a swapchain), is caller-synchronized. Do not rewrite what a
  submitted command still reads; index per-frame resources by the renderer callback's
  `frameIndex` and size rings to `Renderer::MaxFramesInFlight`.
- `Device::waitIdle()`, `Device::submitSync()` and `SwapChain::recreate()` take the queue
  mutex: safe while other threads submit, but they stall the queue. Never call them, or
  anything that submits, presents or waits, inside `interop::withQueue`. External code that
  submits to or waits on the borrowed queue (ImGui's upload path) must run inside
  `withQueue`; passing `interop::queue()` into an init struct needs nothing.
- No backend payload destructor may run while the queue mutex is held: collect garbage
  after unlocking, and bound collection by a serial known to be complete (only `~Device`
  collects everything).

**Errors and fallbacks**

- Failure is reported, never thrown: a factory returns an invalid wrapper with `error()`,
  recording and descriptor calls return `acm::Error` (a rejected call does nothing), and a
  command buffer keeps the first rejection as its sticky `error()`.
- An optional capability the device lacks degrades instead of failing (fill instead of
  wireframe, width 1, isotropic sampling, one mip level, a clamped sample count) and logs a
  warning through the log hook. Library code reports only through the private
  [Log.h](private/archimedes/Log.h) helpers; nothing but `defaultLogHandler` writes to
  stderr.

**MoltenVK and portability** (all guarded on availability, so no-ops on conformant drivers)

- [Instance.cpp](src/vulkan/Instance.cpp): with `InstanceConfig::portability` on (default)
  and `VK_KHR_portability_enumeration` available, enable it and set
  `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR`, or `vkCreateInstance` fails with
  `VK_ERROR_INCOMPATIBLE_DRIVER`. Surface discovery enables every available `*_surface`
  extension (so `VK_EXT_metal_surface` on macOS) except the deprecated `VK_MVK_*_surface`
  ones.
- [Device.cpp](src/vulkan/Device.cpp): enable `VK_KHR_portability_subset` whenever the
  physical device advertises it (spec-required). Its name macro sits behind
  `VK_ENABLE_BETA_EXTENSIONS`, so match the literal string.
- The CPU driver is loaded directly by `acm::Instance` (`VK_LUNARG_direct_driver_loading`);
  do not route it through loader environment variables. MoltenVK stays on the loader's
  discovery through `VK_ICD_FILENAMES`.

**Platform code** (`#ifdef _WIN32`, `<windows.h>`, `dlfcn.h`, `mach-o`) lives only in
[src/Platform.cpp](src/Platform.cpp) (executable location, environment),
[src/PlatformLibrary.cpp](src/PlatformLibrary.cpp) (dynamic libraries) and
[src/vulkan/PlatformSurface.cpp](src/vulkan/PlatformSurface.cpp) (the Win32 headless-surface
fallback). Their private headers expose type-erased handles.

**API shape.** Required construction inputs are explicit factory arguments, or a value type
when several belong together (`createTexture(format, extent, config)`,
`createPipeline(PipelineShaders, target, config)`). A `*Config` struct carries only optional
knobs with production-safe defaults. Do not put required inputs in a `*Config`, and do not
add a `*Config` for a single required choice. `DeviceInfo`, `DeviceOption`, `SurfaceOption`,
`DeviceFeatures`, `DeviceConfig` and the other `acmDeviceInfo.h` structs are plain data, not
handles.

## Conventions

- **C++17** (`target_compile_features(... cxx_std_17)`). Headers use `#pragma once`.
- **Naming:** public classes are `PascalCase` in namespace `acm`, in
  `include/archimedes/acm<Name>.h` and `src/acm<Name>.cpp`. Private Vulkan owners are
  `acm::vulkan::<Name>` in `private/archimedes/vulkan/` and `src/vulkan/`. Public platform
  integration is `acm::interop`.
- **Formatting:** [.clang-format](.clang-format): Allman braces, tabs (width 4), no column
  limit, `All` namespace indentation, left pointer alignment. Use the pinned clang-format 20
  through `cmake --build build --target format` (rewrites the whole file set in place) or
  `--target format-check` (the CI gate); the file set is
  [.clang-format-include](.clang-format-include).
- **Warnings:** our own targets build with `/W4` or `-Wall -Wextra`
  (`acm_warnings()` in [cmake/acmWarnings.cmake](cmake/acmWarnings.cmake)); a change that adds
  a warning in our code is not done.
- **Shaders:** testbed GLSL in [testbed/shaders/](testbed/shaders/), compiled by
  `vk_target_shaders()` for the Vulkan 1.3 target environment. Tests embed SPIR-V
  ([test/test_spirv.h](test/test_spirv.h)) and need no compiler.
- **Tests:** add to the existing files and helpers ([test/vk_test_helpers.h](test/vk_test_helpers.h));
  keep `[gpu]` tests SKIP-aware for missing capabilities. Never duplicate a production path in
  a test, benchmark or example.
- **Keep the docs honest:** a change to an invariant, a command or the public API updates
  this file in the same change; a change to a mechanism updates the matching `docs/` file; a
  limitation added or removed updates [docs/ROUGH-EDGES.md](docs/ROUGH-EDGES.md).

## Build, test, run

Out-of-source only. The first configure fetches pinned dependencies into `.cache/fetch/`
and extracts them under `build/_deps/`. `CMAKE_BUILD_TYPE` defaults to `Release`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Library only (fetches no loader, drivers, GLFW, glslang or Catch2):
`-DARCHIMEDES_BUILD_TESTBED=OFF -DARCHIMEDES_BUILD_TESTING=OFF -DARCHIMEDES_BUILD_BENCHMARK=OFF`.
Other options (validation policy, sanitizers, warnings as errors, driver pins) are listed
in [docs/TOOLING.md](docs/TOOLING.md#cmake-options).

Off Windows, each runnable target gets a launcher, `build/run_<target>.sh`, that points the
loader at the staged drivers:

```sh
./build/run_testbed.sh --list
./build/run_testbed.sh ripple-mesh
./build/run_testbed.sh --device cpu compute-texture
TESTBED_FRAME_CAP=120 ./build/run_testbed.sh compute-texture
./build/run_test-archimedes.sh "[gpu]"
./build/run_bench-archimedes.sh "[fast]"
python3 bench/report.py "[fast]"
python3 bench/report.py "[fast]" --compare build/bench-history.json --json-append build/bench-history.json
```

On Windows there is no launcher; run the binaries directly (`build/testbed/testbed.exe`,
`build/test/test-archimedes.exe`, `build/bench-archimedes.exe`, and
`bench/report.py --exec build/bench-archimedes.exe`). The executables call
`acm::useStagedVulkanRuntime()` themselves, and `acm::Instance` loads the CPU driver
directly, so direct launches from an IDE work everywhere.

Validation: `ACM_VALIDATION=1` turns on the Khronos layer and the debug messenger for every
instance in any binary. The test suite turns it on by itself whenever the loader finds the
layer, and fails the test case that logged a validation error. ctest reports a Catch2 SKIP
as passed; run the test binary directly to see which assertions ran. On Windows the
hardware device skips the swapchain-backed tests (they run on the CPU device); see
[docs/ROUGH-EDGES.md](docs/ROUGH-EDGES.md).

## Where things live

```
include/archimedes/      public API: acm*.h, archimedes.h (umbrella), acmVulkanInterop.h (the
                         one Vulkan seam), acmBackend.h (backend aliases), acmLog.h (log hook)
src/acm*.cpp             public wrapper methods (backend-neutral)
private/archimedes/      ResourcePool.h, DeferredDestroyQueue.h, Log.h, Platform*.h, backendAPI.h
private/archimedes/vulkan/, src/vulkan/
                         the Vulkan backend, one owner per resource; Device owns pools, queue,
                         deferred destruction and the MemoryAllocator (Memory.h)
src/Platform*.cpp, src/vulkan/PlatformSurface.cpp
                         the only platform-specific code
cmake/                   acmVulkan (headers, loader, MoltenVK, SwiftShader staging),
                         acmWarnings, acmSanitize, addGLFW, addGlslang, addcatch2, addclangformat
test/                    Catch2 suite; vk_test_helpers.h, test_spirv*.h, tsan.supp
bench/                   bench-archimedes workloads, report.py, history/
testbed/                 example framework (src/), examples (src/examples/), shaders/
.github/                 CI workflow, add_subdirectory smoke project
docs/                    architecture, build, testing, rough edges, ADRs
```
