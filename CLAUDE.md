# Archimedes

A Vulkan-based simple 2D/3D renderer (C++17). This file is the authoritative
project reference for working in this repo; the general working discipline in
[AGENTS.md](AGENTS.md) still applies and this file overrides it wherever more
specific.

## Status

Early-stage. Today the repo builds a single **static library** (`libarchimedes.a`)
wrapping the core Vulkan objects. There is **no executable, test, or shader
target yet**, and no code path has been run end-to-end against a live driver.
"Done" for the current milestone means *compiles cleanly*, not *renders*.

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
- **Vulkan** — [cmake/addVulkan.cmake](cmake/addVulkan.cmake). Prefers a real
  SDK via `find_package(Vulkan)`; otherwise vendors **Vulkan-Headers** (pinned
  via `VULKAN_HEADERS_VER`) and exposes `Vulkan::Headers`.

### Why only the Vulkan *headers*

"Vulkan" is several separate components. A **static library only needs the
headers to compile** — the loader (`Vulkan::Vulkan`), the MoltenVK ICD, and the
validation layers are link/run-time concerns of an eventual executable, not of
this archive. So the lib links `Vulkan::Headers` (include-only).

The loader + MoltenVK + layers are intentionally **deferred**. When an
executable/test target that creates a real `VkInstance` is added, extend the
clearly marked deferred section at the bottom of
[cmake/addVulkan.cmake](cmake/addVulkan.cmake) to vendor a prebuilt loader +
MoltenVK ICD (+ layers in debug), stage their `*_icd.json` / layer manifests
into `build/`, and export `VK_ICD_FILENAMES` / `VK_LAYER_PATH` for runs — still
without a system install.

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

## Build & verify

Out-of-source only (the top-level `CMakeLists.txt` hard-errors on in-source):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

First configure downloads spdlog and Vulkan-Headers into `thirdparty/`.
`CMAKE_BUILD_TYPE` defaults to `Release`; `Debug` (NDEBUG unset) additionally
compiles the Vulkan validation-layer / debug-messenger paths in
[acmInstance.cpp](src/acmInstance.cpp).

Building anything that actually *runs* Vulkan requires the deferred MoltenVK
runtime stack described above — not available yet.

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
- No tests, examples, or run target exist; correctness beyond "it compiles"
  is unverified against a live driver.
