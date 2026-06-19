# Deferred work

Parked items in priority order, with enough context to pick up later. See
[CLAUDE.md](CLAUDE.md) for the current architecture and the "Known rough edges" list.
Tiers reflect value-to-effort and how likely the next real feature is to need them —
not a commitment to build everything here. The codebase's discipline is "only the
necessary": don't build down this list speculatively.

The original Tier 1 capability gaps (dynamic uniform buffers, storage images, compute
inside the render frame) are **done** — see the bottom of this file.

## Tier A — useful once something demands it

### 1. Device-local storage buffers

`BufferUsage::Storage` is host-visible today (a shader writes it, the CPU reads it back
directly). A device-local + readback-copy path would be faster for GPU-only storage —
matters once compute does heavier work than the demo deform.

### 2. Best-fit allocator

In `Block::tryAllocate` ([src/acmVkMemory.cpp](src/acmVkMemory.cpp)), scan all free
regions and pick the smallest that fits instead of the first. O(regions) already, so no
real cost; reduces fragmentation a little. Self-contained — but only worth doing if
profiling ever shows fragmentation. First-fit is fine at our scale.

### 3. UniformRing for mixed / texture descriptor sets

`acm::UniformRing` bundles per-frame updates only for a *single-uniform* set. The testbed
already hand-rolls a per-frame ring for its mixed uniform+sampler set (and there's no
per-frame ring for texture descriptors), so this is a de-risked convenience extraction,
not new ground.

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
