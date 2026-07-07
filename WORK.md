# Deferred work

Parked items in priority order, with enough context to pick up later. See
[CLAUDE.md](CLAUDE.md) for the current architecture and the "Known rough edges" list.
Tiers reflect value-to-effort and how likely the next real feature is to need them —
not a commitment to build everything here. The codebase's discipline is "only the
necessary": don't build down this list speculatively.

The original Tier 1 capability gaps (dynamic uniform buffers, storage images, compute
inside the render frame) are **done** — see the bottom of this file.

## Current suggested next work

### 1. Pipeline state completeness

Pipeline/render-target format and sample mismatches are guarded during command recording
and surfaced through `acm::Error`; default-state presets exist now, and depth has explicit
test/write/compare state. Remaining polish is arbitrary blend factors plus depth
bias/stencil if a real example needs them.

### 2. Upload/staging performance

`Buffer::write` and `Texture::upload` are correct but load-time oriented: staging +
one-shot command + wait. Start with persistent upload command resources or an upload
ring if streaming assets or per-frame uploads become real requirements.

### 3. Memory allocator refinement

Best-fit allocation is small and contained in `src/vulkan/Memory.cpp`. Block
reclamation is useful later. Full defragmentation should wait until real memory
pressure appears.

### 4. Texture API completeness

Missing pieces include explicit mip regeneration, partial/level uploads, renderable
mipmapped textures, and better mip filtering. This is a natural feature area once
texture-heavy examples demand it.

## Tier A — useful once something demands it

### 1. Device-local storage buffers

`BufferUsage::Storage` is host-visible today (a shader writes it, the CPU reads it back
directly). A device-local + readback-copy path would be faster for GPU-only storage —
matters once compute does heavier work than the demo deform.

### 2. Best-fit allocator

In `Block::tryAllocate` ([src/vulkan/Memory.cpp](src/vulkan/Memory.cpp)), scan all free
regions and pick the smallest that fits instead of the first. O(regions) already, so no
real cost; reduces fragmentation a little. Self-contained — but only worth doing if
profiling ever shows fragmentation. First-fit is fine at our scale.

### 3. General per-frame descriptor resources

Callers currently keep their own per-frame buffers and descriptor sets, indexed by the
renderer callback's fence-safe frame slot. If that pattern repeats enough to justify an
abstraction, it should support arbitrary descriptor layouts and resource types rather
than special-casing a single uniform binding.

### 4. Drop the deform's `submitSync` wait (compute pipelining)

The deform's `submitSync` still waits the compute idle each frame (for the transient
command-pool lifetime + the time uniform). True CPU/GPU overlap needs a persistent compute
command pool + a fence ring (and a ring of the time uniform), so the main thread can submit
the next deform without waiting the last idle. The barriers already handle the GPU-side
ordering — this is purely about removing the remaining CPU stall, a smoothness win the demo
doesn't currently feel. (This is the renderer's only remaining per-frame device wait; the
top-of-loop `waitIdle` was removed when the barriers landed. Note the new
`Renderer::render(prePass, record)` hook is the cleaner home for per-frame compute in a
single-window app — it folds the dispatch into the frame's command buffer with no extra
submit. The two-window testbed can't use it, though: its two windows render the one shared
mesh in two independent submits, so the deform stays a single main-thread dispatch.)

## Tier B — speculative / large / explicitly deferred

### 5. Dynamic window add/remove

The example framework now supports any fixed window count an example asks for (1..N). What
remains is adding/removing windows *at runtime* (the `App` builds its windows/contexts once
up front). Do it if an example ever needs it.

### 6. Per-queue submission

Create the device with several queues from the family and give each `Renderer` its own,
removing the submit mutex. Needs a multi-queue `Device` API and a family with enough
queues; the mutex is uncontended and always works, so this only pays off under contention
that doesn't exist.

### 7. Block reclamation

Track per-block used bytes; when a block goes fully free, `vkFreeMemory` it (or keep one
spare). Only matters for long-running sessions that churn memory.

### 8. Async compute queue

A dedicated compute queue family overlapping with graphics. Advanced; blocked on the same
multi-queue `Device` work as #6.

### 9. Input attachments

New `DescriptorType` + subpass wiring for deferred/subpass rendering — not on the roadmap.

### 10. Defragmentation

Relocate live resources to compact blocks: re-create images/buffers at new offsets and
rebind everything referencing them (descriptor sets, framebuffers). Only pays off under
real memory pressure this renderer doesn't generate. **Don't build speculatively.**

---

## Done (for reference)

- **CI** — `.github/workflows/ci.yml` runs the deterministic hosted checks:
  clang-format, library-only configure/build, full configure/build, and `ctest` on
  Linux, macOS, and Windows. It keeps live Vulkan rendering as a separate future lane
  for self-hosted runners with real graphics hardware. There is also an
  `add_subdirectory` smoke project under `.github/subproject-smoke`.
- **Public README guide** — `README.md` now gives the public front door: build and
  integration commands, CI-tested platforms, feature summary, testbed examples,
  first-draw/texture/compute API sketches, tests, and benchmarks. Deeper architecture
  remains in `CLAUDE.md`.
- **Feature negotiation API** — `DeviceConfig` lets callers distinguish required and
  optional curated device features. The default still enables every supported known
  feature; unavailable required features make `createDevice` return an invalid `Device`
  with an `acm::Error`. Proved by `test_device.cpp`.
- **Pipeline config presets** — `PipelineConfig::Default()` names the unchanged
  smoke-test defaults, while `Mesh3D()`, `Sprite2D()`, and `Wireframe(width)` provide
  opt-in starting points without silently changing existing pipeline behavior. Proved
  by `test_pipeline_state.cpp`; `Sprite2D` and `Wireframe` are used by testbed examples.
- **Testbed example framework** — the testbed is a runner (`App`) + swappable `Example`
  plugins selected by name (`testbed <name>` / `--list`), generalised to 1..N windows.
  Six examples give live-driver coverage: `ripple-mesh` (multi-window + threads + compute
  deform), `instanced-cubes` (dynamic uniforms), `compute-texture` (storage image +
  `render(prePass)`), `mirror` (render-to-texture), `sprite-atlas` (descriptor array + alpha
  blend), `wireframe` (polygon mode + wide lines). See the Testbed section of CLAUDE.md.
  (This also delivered the 1..N-window generalisation; only *dynamic* add/remove of windows
  at runtime remains under "N windows" below.)
- **Dynamic uniform buffers** — `DescriptorType::UniformBufferDynamic`,
  `DescriptorSet::setDynamicBuffer`, `CommandBuffer::bindDescriptorSet(.., dynamicOffset)`,
  `Device::minUniformBufferOffsetAlignment()`. One buffer holds many objects' constants;
  the per-draw offset picks one. Proved by `test_dynamic_uniform.cpp`.
- **Storage images** — `DescriptorType::StorageImage`, `Texture` `storage` usage,
  `DescriptorSet::setStorageImage`, and `CommandBuffer::transitionImage` (a wrapped image
  barrier over a neutral `acm::ImageLayout`). A compute shader writes an image; proved by
  `test_storage_image.cpp`.
- **Compute inside the render frame** — `Renderer::render(prePass, record)` records compute
  / barriers / transitions before the render pass, in the frame's command buffer (no extra
  submit). Proved by `test_renderer.cpp` (SKIPs without a headless surface).
- **Compute pipeline** — `acm::ComputePipeline` + `Device::createComputePipeline`,
  `CommandBuffer::bindComputePipeline` / `bindComputeDescriptorSet` / `dispatch`,
  `ShaderStage::Compute`, `Device::submitSync`. Proved by `test_compute.cpp`; the testbed
  mesh deform is `mesh.comp`.
- **Wrapped pipeline barrier** — `CommandBuffer::bufferBarrier(buffer, srcStage, dstStage)`
  (conservative shader read|write dependency). Proved by `test_compute.cpp` (compute write
  → fragment read in one command buffer); the testbed brackets its deform dispatch with it
  to sync the shared SSBO across submission order instead of a device wait-idle.
- **Multi-window** — two windows sharing one `Instance` + `Device`, each with its own
  `Surface` / `SwapChain` / `Renderer` on its own thread (fork-join). Core change was
  `Device::deviceMutex()` (serializing the one `VkQueue` + frame/graveyard bookkeeping) +
  `Device::waitIdle()`; the rest is the testbed `Scene`/`View`/`RenderWorker` split.
- **Descriptors (core)** — storage buffers, multi-stage bindings (`ShaderStage` flag set),
  descriptor arrays (`DescriptorBinding::count`).
