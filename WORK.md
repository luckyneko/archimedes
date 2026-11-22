# Deferred work

Parked items in priority order, with enough context to pick up later. The architecture is
in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and the current limitations in
[docs/ROUGH-EDGES.md](docs/ROUGH-EDGES.md); this file is what we might do about them. Tiers
reflect value-to-effort and how likely the next real feature is to need them, not a
commitment to build everything here. The codebase's discipline is "only the necessary":
don't build down this list speculatively.

## Current suggested next work

### 1. First runs of the validation and ThreadSanitizer lanes

The `Linux validation` and `Linux TSan` CI lanes have never run. Run them, fix or triage
what they report (sync validation is informational and may report write-after-read hazards
on re-rendered offscreen targets), then decide whether sync validation should gate. Once
the Linux and macOS lanes have had a clean run with `ARCHIMEDES_WARNINGS_AS_ERRORS=ON`, turn
it on there too; today only the Windows lane sets it.

### 2. Command-buffer resource retention

Command buffers do not retain what they record, so a resource released before its
submission can be destroyed early ([docs/ROUGH-EDGES.md](docs/ROUGH-EDGES.md)). The rule is
documented; whether to retain instead is an open design question. The candidate: the
backend command buffer retains each referenced wrapper until its next `begin()` or reset,
and the renderer releases them after the frame fence. Weigh that against the per-call cost
on the recording path, which is lock-free today.

### 3. Hardware swapchain coverage on Windows

On Windows the hardware device skips every swapchain-backed test, because the headless
surface comes from the directly loaded SwiftShader's `VK_EXT_headless_surface`, which the
Windows hardware drivers do not support. Options: let a caller (or the test helpers) ask
`createHeadlessSurface` for the off-screen Win32 window path, or build the surface per
device kind. Until then the testbed is the hardware path's only coverage there.

### 4. Pipeline state completeness

Pipeline/render-target format and sample mismatches are guarded during command recording
and surfaced through `acm::Error`; default-state presets exist, and depth has explicit
test/write/compare state. What remains is arbitrary blend factors plus depth bias and
stencil, if a real example needs them.

### 5. Upload/staging performance

`Buffer::write` and `Texture::upload` are correct but load-time oriented: staging plus a
one-shot command plus a wait. Start with persistent upload command resources or an upload
ring if streaming assets or per-frame uploads become real requirements.

### 6. Texture API completeness

Missing pieces: a format-capability check at creation (does the format support the
requested usage with optimal tiling), explicit mip regeneration, partial and per-level
uploads, renderable mipmapped textures, and better mip filtering. A natural feature area
once texture-heavy examples demand it.

## Tier A — useful once something demands it

### 1. Device-local storage buffers

`BufferUsage::Storage` is host-visible today (a shader writes it, the CPU reads it back
directly). A device-local buffer plus a readback copy would be faster for GPU-only storage,
which matters once compute does heavier work than the demo deform.

### 2. Memory allocator refinement

All in [src/vulkan/Memory.cpp](src/vulkan/Memory.cpp). Best-fit in `Block::tryAllocate`
(scan every free region for the smallest fit instead of the first; O(regions) already, so
no real cost) reduces fragmentation a little, but is only worth it if profiling shows
fragmentation. Block reclamation (track per-block used bytes and `vkFreeMemory` a block
that goes fully free, or keep one spare) matters only for long-running sessions that churn
memory.

### 3. General per-frame descriptor resources

Callers keep their own per-frame buffers and descriptor sets, indexed by the renderer
callback's fence-safe frame slot. If that pattern repeats enough to justify an abstraction,
it should support arbitrary descriptor layouts and resource types rather than
special-casing a single uniform binding.

### 4. Remove the testbed's per-frame waits

The `ripple-mesh` deform runs through `submitSync` each frame, which waits the queue idle
(for the transient command pool and the time uniform). True CPU/GPU overlap needs a
persistent compute command pool, a fence ring and a ring of the time uniform, so the main
thread can submit the next deform without waiting for the last. The barriers already handle
the GPU-side ordering; this only removes a CPU stall the demo doesn't currently feel. The
two-window example cannot use `Renderer::render(prePass, record)`, the cleaner home for
per-frame compute in a single-window app, because its two windows draw one shared mesh in
two independent submits. Separately, `instanced-cubes`, `mirror` and `wireframe` call
`Device::waitIdle()` in `onUpdate` before rewriting a single per-frame resource; a ring
indexed by `frameIndex`, as `compute-texture` uses, would remove those stalls.

### 5. Present and device-loss robustness

Use `VK_EXT_swapchain_maintenance1` present fences where available, so destroying present
semaphores no longer relies on a device-idle wait covering presents. Decide what the
renderer should do when both a frame's submit and its fallback empty submit fail (a lost
device), which today can leave that slot's acquire semaphore signalled.

## Tier B — speculative / large / explicitly deferred

### 6. Dynamic window add/remove

The example framework supports any fixed window count an example asks for (1..N). What
remains is adding and removing windows at runtime (the `App` builds its windows and
contexts once, up front). Do it if an example ever needs it.

### 7. Per-queue submission

Create the device with several queues from the family and give each `Renderer` its own,
removing the shared queue mutex. Needs a multi-queue `Device` API and a family with enough
queues; the mutex is rarely contended, so this only pays off under contention that doesn't
exist yet.

### 8. Async compute queue

A dedicated compute queue family overlapping with graphics. Advanced; blocked on the same
multi-queue `Device` work as #7.

### 9. Input attachments

A new `DescriptorType` plus subpass wiring for deferred/subpass rendering. Not on the
roadmap.

### 10. Defragmentation

Relocate live resources to compact blocks: re-create images and buffers at new offsets and
rebind everything that references them (descriptor sets, render targets). Only pays off
under real memory pressure this renderer doesn't generate. **Don't build speculatively.**

---

## Done (for reference)

- **Review fixes (wave 1)** — handles read lock-free from one atomic word and pools sweep
  only what died; the deferred destroy queue collects in one ordered pass. An acquired
  frame always completes, a recording error presents a cleared frame and is returned, and
  swapchain recreation is safe while other renderers submit (present semaphores per image,
  `waitIdle` under the queue mutex). Memory blocks are split by linear and non-linear
  kind; `Texture::upload` and `Buffer::write` validate their input; the mip count is fixed
  at creation. Every recording and descriptor call returns `acm::Error`, descriptor sets
  retain what they point at, and an invalid `Device` is safe to call. A log hook carries
  the library's warnings and the debug messenger's messages (which used to go straight to
  stderr), `ACM_VALIDATION` validates any binary, and the suite fails on validation errors,
  with new validation and ThreadSanitizer CI lanes.
- **Warnings and usage checks** — `cmake/acmWarnings.cmake` (`/W4`, `-Wall -Wextra`,
  `ARCHIMEDES_WARNINGS_AS_ERRORS`, on in the Windows lane); feature, sample-count and
  mip-chain fallbacks log a warning; buffer binds, buffer and storage-image descriptor
  writes and readback copies check usage; compute descriptor binds check layout
  compatibility.
- **CI** — `.github/workflows/ci.yml` runs clang-format, library-only and full builds plus
  `ctest` on Linux, macOS and Windows, with the `[gpu]` tests on the staged CPU device; the
  validation and TSan lanes; and an `add_subdirectory` smoke project under
  `.github/subproject-smoke`. Hardware-GPU coverage comes from local runs.
- **CPU device** — SwiftShader staged beside every runnable target and loaded directly by
  `acm::Instance` ([ADR-0001](docs/adr/0001-cpu-device-via-staged-swiftshader.md)); `[gpu]`
  tests run once per device kind.
- **Public README guide** — `README.md` is the public front door: build and integration
  commands, CI-tested platforms, feature summary, testbed examples, API sketches, tests and
  benchmarks. Depth lives in `docs/`.
- **Feature negotiation API** — `DeviceConfig` distinguishes required and optional curated
  device features. The default enables every supported known feature; an unavailable
  required feature makes `createDevice` return an invalid `Device` with an `acm::Error`.
  Proved by `test_device.cpp`.
- **Pipeline config presets** — `PipelineConfig::Default()` names the smoke-test defaults;
  `Mesh3D()`, `Sprite2D()` and `Wireframe(width)` are opt-in starting points. Proved by
  `test_pipeline_state.cpp`; `Sprite2D` and `Wireframe` are used by testbed examples.
- **Testbed example framework** — a runner (`App`) plus swappable `Example` plugins selected
  by name (`testbed <name>` / `--list`), for 1..N windows, with six examples
  ([docs/TESTING.md](docs/TESTING.md#testbed)).
- **Dynamic uniform buffers** — `DescriptorType::UniformBufferDynamic`,
  `DescriptorSet::setDynamicBuffer`, `CommandBuffer::bindDescriptorSet(.., dynamicOffset)`,
  `Device::minUniformBufferOffsetAlignment()`. Proved by `test_dynamic_uniform.cpp`.
- **Sampled images** — `DescriptorType::SampledImage`, `DescriptorSet::setSampledImage` and
  `acm::interop::descriptorSet`, so an external renderer (lain's ImGui GUI) can bind a
  descriptor Archimedes owns and defers destroying. Proved by `test_descriptors.cpp`.
- **Storage images** — `DescriptorType::StorageImage`, `TextureConfig::storage`,
  `DescriptorSet::setStorageImage` and `CommandBuffer::transitionImage`. Proved by
  `test_storage_image.cpp`.
- **Compute inside the render frame** — `Renderer::render(prePass, record)` records compute,
  barriers and transitions before dynamic rendering, in the frame's command buffer. Proved
  by `test_renderer.cpp`.
- **Compute pipeline** — `acm::ComputePipeline`, `Device::createComputePipeline`,
  `bindComputePipeline` / `bindComputeDescriptorSet` / `dispatch`, `ShaderStage::Compute`,
  `Device::submitSync`. Proved by `test_compute.cpp`; the testbed deform is `mesh.comp`.
- **Wrapped pipeline barrier** — `CommandBuffer::bufferBarrier(buffer, srcStage, dstStage)`.
  Proved by `test_compute.cpp`; the testbed brackets its deform with it instead of a device
  wait-idle.
- **Multi-window** — windows sharing one `Instance` and `Device`, each with its own
  `Surface`, `SwapChain` and `Renderer` on its own thread. The core is the device's queue
  mutex serializing the one `VkQueue`, plus submission-serial deferred destruction; the rest
  is the testbed's `Scene` / `RenderWorker` split.
- **Descriptors (core)** — storage buffers, multi-stage bindings (`ShaderStage` flags) and
  descriptor arrays (`DescriptorBinding::count`).
