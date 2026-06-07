# Archimedes

A Vulkan-based simple 2D/3D renderer (C++17). This file is the authoritative
project reference for working in this repo; the general working discipline in
[AGENTS.md](AGENTS.md) still applies and this file overrides it wherever more
specific.

## Status

Early-stage. The repo builds the core **static library** (`libarchimedes.a`)
plus an optional **`testbed/` executable** that drives the API against a real
GLFW window. The testbed creates instance → surface → device → swapchain and
**renders a triangle** (pipeline + SPIR-V shaders + per-frame sync + present)
on a live MoltenVK driver — verified end-to-end on Apple Silicon. A Catch2
test suite covers handle semantics + headless instance/device creation. There
is no swapchain-recreation path yet (the window is fixed size; see Known rough
edges).

## Architecture

### The `acm::` handle pattern

Every public class (`Instance`, `Surface`, `Device`, `SwapChain`,
`RenderTarget`, `Image`) is a thin value-type **handle** over a pImpl held by
`std::shared_ptr<impl>`:

```cpp
class Thing {
public:
    Thing() {}                       // empty/null handle
    Thing(/* real args */);          // constructs impl, only assigns m on full success
    void reset() { m.reset(); }
    bool valid() const { return m != nullptr; }
    /* accessors forward to m-> */
private:
    struct impl; std::shared_ptr<impl> m;
};
```

Conventions that come with the pattern — match them in any new class:

- **Copyable, shareable, cheap.** Handles are passed by value; the `shared_ptr`
  gives shared ownership of the underlying Vulkan object. The `impl`'s
  destructor owns Vulkan teardown (`vkDestroy*`), so lifetime is automatic and
  ref-counted. Do not add manual destroy calls on the handle.
- **All-or-nothing construction.** The constructor builds a local
  `auto impl = std::make_shared<impl>();`, does the Vulkan work, and assigns
  `m = impl;` **only at the very end**. Any failure path `return`s early,
  leaving the handle `!valid()`. Never assign `m` before the object is fully
  built.
- **`impl` keeps its dependencies alive** by storing the handles it was built
  from (e.g. `Device::impl` stores its `acm::Instance`, `Surface::impl` stores
  its `acm::Instance`). This guarantees correct teardown order via shared_ptr
  refcounts — a child never outlives its parent's Vulkan object.
- **Forward declarations** live in [acmForward.h](include/archimedes/acmForward.h);
  include it (not the full headers) when you only need to name a handle type.

### Object graph / ownership

```
Instance ── enumerates ──> GPU[] (physical devices, queue families)
   │
   ├── Surface(Instance, VkSurfaceKHR)   // platform window surface + per-GPU support query
   │
   └── Device(Instance, GPU, queueIdx)   // logical device + queue
          │
          └── SwapChain(Device, Surface, format, presentMode)
                 └── RenderTarget(Device, Image[])   // image views + render pass + framebuffer
                        └── Image(Device, VkImage, VkImageCreateInfo)
```

`GPU`, `GPUQueueFamily`, and `GPUSurfaceSupport` ([acmGPU.h](include/archimedes/acmGPU.h))
are plain data structs, not handles. `Version` ([acmVersion.h](include/archimedes/acmVersion.h))
carries the engine version; `acm::VERSION` is the engine's own.

`Image` currently only wraps **externally owned** swapchain images
(`externallyOwned = true`, so its destructor does not `vkDestroyImage`). The
self-creating path is stubbed out in [acmImage.cpp](src/acmImage.cpp) and not
yet wired up.

[acmUtils.h](include/archimedes/acmUtils.h) holds free helpers in
`acm::Utils` (e.g. `logGPUSupport`).

## Dependencies (vendored, no system install)

Both deps are downloaded on demand into `thirdparty/` (git-ignored) by CMake
modules under [cmake/](cmake/), mirroring the pattern used in the sibling
`thorax` repo. Nothing is installed system-wide.

- **spdlog** — [cmake/addspdlog.cmake](cmake/addspdlog.cmake) (copied from
  thorax). Provides `spdlog::spdlog`. Logging surface used throughout.
- **Vulkan headers** — [cmake/addVulkan.cmake](cmake/addVulkan.cmake). Prefers a
  real SDK via `find_package(Vulkan)`; otherwise vendors **Vulkan-Headers**
  (pinned via `VULKAN_HEADERS_VER`) and exposes `Vulkan::Headers`. This is all
  the **library** needs.
- **Vulkan runtime** (testbed only) —
  [cmake/addVulkanRuntime.cmake](cmake/addVulkanRuntime.cmake). Builds
  **Vulkan-Loader** from source against the vendored headers (→ `Vulkan::Loader`)
  and downloads a prebuilt **MoltenVK** ICD. Provides
  `acm_stage_vulkan_runtime(<target>)`.
- **GLFW** (testbed only) — [cmake/addGLFW.cmake](cmake/addGLFW.cmake). Windowing
  + Vulkan surface; exposes `glfw`.
- **glslang** (testbed only) — [cmake/addGlslang.cmake](cmake/addGlslang.cmake).
  Offline GLSL→SPIR-V compiler: prefers system `glslc`/`glslangValidator`, else
  builds glslang's standalone from source (`ENABLE_OPT=OFF`, so no SPIRV-Tools).
  Defines the `vk_target_shaders(<target> <sources…>)` helper, which compiles to
  `<bindir>/shaders/*.spv`. Note: it deliberately does **not** cache its
  compiler choice behind a guard around the vendored `add_subdirectory` — that
  target must be re-created every configure.
- **Catch2** (tests only) — [cmake/addcatch2.cmake](cmake/addcatch2.cmake)
  (copied from thorax). Provides `Catch2::Catch2WithMain` + `catch_discover_tests`.

### Why the library links only the Vulkan *headers*

"Vulkan" is several separate components. A **static library only needs the
headers to compile** — the loader, the MoltenVK ICD, and the validation layers
are link/run-time concerns of an executable, not of this archive. So
`libarchimedes` links `Vulkan::Headers` (include-only) and leaves its `vk*`
symbols unresolved; the **testbed** links `Vulkan::Loader` to resolve them.

### Testbed runtime (fully vendored, no system install)

[cmake/addVulkanRuntime.cmake](cmake/addVulkanRuntime.cmake) realizes the
"no system install" goal end to end:

- **Loader**: built from the pinned `Vulkan-Loader` source (codegen off, so no
  Python). Its second Apple-only `vulkan-framework` target is set
  `EXCLUDE_FROM_ALL` since we only link the `vulkan` dylib.
- **MoltenVK**: the prebuilt `MoltenVK-macos.tar` release; `libMoltenVK.dylib` +
  `MoltenVK_icd.json` (relative `./libMoltenVK.dylib`, `is_portability_driver`).
- **`acm_stage_vulkan_runtime(<target>)`**: POST_BUILD-copies the ICD + dylib to
  `<bindir>/vulkan/`, and generates `build/run_<target>.sh` that exports
  `VK_ICD_FILENAMES` (and `VK_LAYER_PATH` *only if* `$VULKAN_SDK` is present —
  macOS ships validation layers only via the LunarG SDK, so they stay optional).
- GLFW is bound to our vendored loader via
  `glfwInitVulkanLoader(vkGetInstanceProcAddr)` (so it doesn't dlopen a system
  libvulkan).

## MoltenVK / portability

On macOS, Vulkan runs through **MoltenVK** (a portability driver). The code
already accounts for the two non-obvious requirements; preserve them:

- **Instance** ([acmInstance.cpp](src/acmInstance.cpp)): when
  `VK_KHR_portability_enumeration` is available, it is enabled (along with
  `VK_KHR_get_physical_device_properties2`) and the
  `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR` flag is set — otherwise
  `vkCreateInstance` returns `VK_ERROR_INCOMPATIBLE_DRIVER`.
- **Device** ([acmDevice.cpp](src/acmDevice.cpp)): `VK_KHR_portability_subset`
  is enabled whenever the physical device advertises it (spec-required). The
  name macro lives behind `VK_ENABLE_BETA_EXTENSIONS`, so the literal string is
  matched instead.

Both are guarded on availability, so they are no-ops on conformant drivers.

## Testbed ([testbed/](testbed/))

A runnable harness, ported from the `urdr` project's pattern, split into a
reusable framework and swappable test content:

- **`Window`** ([testbed/src/Window.cpp](testbed/src/Window.cpp)) — owns the GLFW
  window and the `acm::Surface`/`Device`/`SwapChain` lifecycle (same
  handle/all-or-nothing shape as `acm::`), driving a delegate per frame.
- **`WindowDelegate`** ([testbed/src/WindowDelegate.h](testbed/src/WindowDelegate.h))
  — the interface a demo implements: `onSelectSwapChainSettings` (pick
  GPU/queue/format), `onInit`/`onShutdown`/`onUpdate`/`onRender`.
- **`MainDelegate`** ([testbed/src/MainDelegate.cpp](testbed/src/MainDelegate.cpp))
  — the current demo: builds a graphics pipeline from the `triangle.{vert,frag}`
  shaders (loaded from the `TESTBED_SHADER_DIR` compile-definition path), records
  one command buffer per swapchain render target, and draws/presents a triangle
  with `MAX_FRAMES_IN_FLIGHT = 2` per-frame sync. Evolve this in place for new
  demos — do not fork a parallel delegate.

Shaders live in [testbed/shaders/](testbed/shaders/) and are compiled by
`vk_target_shaders` to `build/testbed/shaders/*.spv`; the app finds them via the
`TESTBED_SHADER_DIR` define wired in [testbed/CMakeLists.txt](testbed/CMakeLists.txt).

## Tests ([test/](test/))

Catch2 suite, gated by `ARCHIMEDES_BUILD_TESTING` (default ON top-level). The
library is a thin wrapper over `vk*`, so coverage splits in two:

- **Pure unit** (`[acm]`, `[handle]`, `[version]`) — handle validity/reset/share
  semantics and `Version`. No driver needed.
- **Integration** (`[gpu]`) — all headless via a **headless surface**
  (`vkCreateHeadlessSurfaceEXT` — no window): `acm::Instance`/`Device` creation +
  GPU enumeration, `acm::Surface` per-GPU support, and `acm::SwapChain` extent
  clamping. `test/CMakeLists.txt` points `VK_ICD_FILENAMES` at the vendored
  MoltenVK ICD (`ACM_MOLTENVK_ICD`) for ctest. Each `[gpu]` test `SKIP`s (not
  fails) when no driver / extension / capability is present, so a GPU-less CI
  stays green. (Note: ctest reports a Catch2 `SKIP` as "Passed"; run the binary
  with the ICD env to confirm assertions actually execute.)

Shared `[gpu]` scaffolding (headless-surface + graphics-GPU selection) lives in
[test/vk_test_helpers.h](test/vk_test_helpers.h) — reuse it, don't re-roll it.
Add new tests in place; do not duplicate the production path.

## Build & verify

Out-of-source only (the top-level `CMakeLists.txt` hard-errors on in-source):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

First configure downloads spdlog + Vulkan-Headers and, when the testbed is
enabled (`ARCHIMEDES_BUILD_TESTBED`, default ON for top-level builds), GLFW +
Vulkan-Loader + MoltenVK + glslang into `thirdparty/` (git-ignored). `CMAKE_BUILD_TYPE`
defaults to `Release`; `Debug` (NDEBUG unset) additionally compiles the Vulkan
validation-layer / debug-messenger paths in [acmInstance.cpp](src/acmInstance.cpp).

Run the testbed via the generated launcher (it sets `VK_ICD_FILENAMES` to the
staged MoltenVK ICD):

```sh
./build/run_testbed.sh
```

Run the tests with ctest from the build dir:

```sh
ctest --test-dir build --output-on-failure
```

`-DARCHIMEDES_BUILD_TESTBED=OFF` / `-DARCHIMEDES_BUILD_TESTING=OFF` build only
the library (headers only — no loader/MoltenVK/GLFW/glslang/Catch2 downloads).

## Conventions

- **C++17.** `target_compile_features(... cxx_std_17)`.
- **Headers** use `#pragma once` (no include guards).
- **Naming:** classes `PascalCase` in namespace `acm`; files `acm<Name>.{h,cpp}`;
  the pImpl struct is always `struct impl`, the member always `m`. Vulkan
  accessors are named `vk<Thing>()` (e.g. `vkInstance()`, `vkDevice()`).
- **Formatting:** [.clang-format](.clang-format) — Allman braces, tabs (width 4),
  no column limit, `All` namespace indentation, left pointer alignment. Run
  clang-format (v21) on touched files; `editor.formatOnSave` is on in VS Code.
- **Shaders:** the `vk_target_shaders()` helper in `CMakeLists.txt` compiles
  GLSL→SPIR-V via `glslc`. No shaders or `glslc` present yet; it is dormant.

## Known rough edges (pre-existing, not yet addressed)

- [acmSwapChain.cpp](src/acmSwapChain.cpp): `pQueueFamilyIndices` is set to a
  bogus non-null pointer; harmless only because `queueFamilyIndexCount == 0`.
- [acmImage.cpp](src/acmImage.cpp): the image-creating constructor is commented
  out; `Image` only wraps borrowed swapchain images today.
- **No swapchain recreation.** The testbed window is forced non-resizable; a
  minimized/out-of-date swapchain (`VK_ERROR_OUT_OF_DATE_KHR`) is not handled —
  acquire/present return values are currently ignored.
- The triangle pipeline uses `VK_CULL_MODE_NONE` so winding can't hide it (a
  smoke-test choice, not a considered default).
- Test coverage is still shallow: handle semantics + headless instance / device
  / surface / swapchain only. No render-pass/pipeline/render-loop tests, and the
  `[gpu]` tests need a Metal-capable driver (they SKIP otherwise).
