# Architecture

How the Archimedes library is put together. [CLAUDE.md](../CLAUDE.md) states the rules a
change must not break and the build commands; this file explains the mechanisms behind
them. Build, vendoring and CI are in [TOOLING.md](TOOLING.md); the test suite, validation,
testbed and benchmarks in [TESTING.md](TESTING.md); known limitations in
[ROUGH-EDGES.md](ROUGH-EDGES.md); vocabulary (hardware device, CPU device, CPU driver,
staged driver) in [CONTEXT.md](../CONTEXT.md).

## Layers

- **Public wrappers**: `include/archimedes/acm*.h`, implemented in the matching
  `src/acm*.cpp`. No Vulkan types. Each wrapper translation unit includes
  [backendAPI.h](../private/archimedes/backendAPI.h), which selects the complete backend
  through [vulkan/API.h](../private/archimedes/vulkan/API.h) (`vulkan/Resources.h`
  aggregates only the resource owners). Public classes are forward-declared in
  [acmForward.h](../include/archimedes/acmForward.h).
- **Backend selection**: [acmBackend.h](../include/archimedes/acmBackend.h) forward-declares
  the `acm::vulkan` classes and aliases them as `acm::backend::T` without including Vulkan.
  It is the only place backend declarations appear in public headers.
- **Vulkan backend**: one private owner per resource, `private/archimedes/vulkan/T.h` plus
  `src/vulkan/T.cpp`, holding its construction, native state, operations, dependency
  retention and destruction. Backend files include their Vulkan siblings directly.
  [Convert.cpp](../src/vulkan/Convert.cpp) maps the neutral enums of
  [acmTypes.h](../include/archimedes/acmTypes.h) to Vulkan.
- **Interop seam**: [acmVulkanInterop.h](../include/archimedes/acmVulkanInterop.h)
  (`acm::interop`), the one public header that includes `<vulkan/vulkan.h>`. It returns
  native handles for external Vulkan code (ImGui's backend, for one), adopts external
  surfaces (`adoptSurface`) and runs raw queue work under the device queue mutex
  (`withQueue`). Every accessor returns a null handle for an invalid or stale wrapper.

## Handles and resource pools

Every public resource wrapper (`Buffer`, `Texture`, `Pipeline`, `CommandBuffer`, ...) holds
one `acm::ResourceRef<backend::T>`
([acmResourceRef.h](../include/archimedes/acmResourceRef.h)): a stable pointer to a
`ResourceSlot<T>` ([acmResourceSlot.h](../include/archimedes/acmResourceSlot.h)) plus the
`ResourceSlotID<T>` (index and generation) it was issued for. There are no per-resource
pImpls and no virtual functions.

**Slots.** A `ResourceSlot<T>` composes an optional, move-only backend payload with its
lifetime state, one 64-bit atomic word: generation in the high 32 bits, reference count in
the low 32. The payload does not inherit from the slot.

- `valid()` and `access()` are one acquire load and a compare, with no lock and no write.
  `access()` returns the payload, or `nullptr` for a stale or invalid reference. There is
  no `operator->`, so wrapper code branches on the pointer before calling the backend.
- A copy retains with one compare-exchange and a release is one compare-exchange. The
  release that drops the count to zero retires the lifetime, moving the word from
  (g, n > 0) to (g + 1, 0), so exactly one thread retires each lifetime.
- `forceInvalidate()` makes the same transition whatever the count, so every outstanding
  wrapper goes stale at once. A stale reference only compare-exchanges a word whose
  generation matches its own and whose count is non-zero, so its later release is
  harmless and it never writes a retired or reused slot.
- A stale reference could match again only after 2^32 lifetimes of one slot (asserted;
  the wrap skips generation 0, which a default ID carries).
- A backend payload stores a borrowed pointer to its owner (the device or instance) only
  when its operations need one.
- The slot's pool-side operations (initialize, emplace, start a lifetime, take the payload,
  list links) are private, with `ResourcePool<T>` as its friend.

**Pools.** [ResourcePool.h](../private/archimedes/ResourcePool.h) is owner-agnostic,
block-based storage: blocks of 256 slots that never move, so a slot pointer stays valid
until the owning root destroys the pool.

- `emplace(construct)` takes a slot from an intrusive free list (under the pool mutex),
  constructs the payload in place and starts a lifetime. A payload that reports
  `!valid()` has its `error()` captured, is destroyed and its slot recycled at once; the
  caller receives that error and an invalid wrapper.
- A retiring slot pushes itself onto the pool's lock-free retired list as its last touch
  of the slot. Retiring never takes the pool mutex, so a `ResourceRef` never needs to know
  its pool.
- `collectGarbage(collect)` returns after one relaxed load when nothing died. Otherwise it
  claims the whole retired list with one exchange, hands each payload to `collect` in
  retirement order, then returns the emptied slots to the free list under one lock. The
  cost grows only with what died, and nothing allocates. Concurrent sweeps claim disjoint
  slots. If `collect` throws, the swept part is recycled, the rest is republished and the
  exception propagates.
- A dead slot is reused only after a sweep has handed its payload to the collection policy.
- `clear(collect)` force-retires every live slot under the pool mutex (O(slots), for root
  teardown) and sweeps them, handing payloads over in slot order.

**Deferred destruction.** [DeferredDestroyQueue.h](../private/archimedes/DeferredDestroyQueue.h)
holds type-erased payloads tagged with a queue submission serial. `collect(completedSerial)`
takes every ready entry in one linear pass under its mutex, keeping the rest in enqueue
order, then destroys the ready ones after unlocking, in enqueue order. A destructor may
enqueue more (they wait for a later collect) or start a nested collect.

**Device collection.** `vulkan::Device::collectGarbage(serial)` sweeps every device pool,
moving each payload into the deferred queue tagged with the last submitted serial, then
collects the queue up to `serial`. A released resource is therefore destroyed only after
every submission made before its release has completed. Collection runs after each frame
fence wait, after `submitSync` and the internal one-shot uploads, in `Device::waitIdle()`
and in `SwapChain::recreate()`, each bounded by a serial known to be complete. Only
`~Device` collects everything (`UINT64_MAX`), after clearing every pool, until the queue is
empty. `vulkan::Instance` clears its surface pool at destruction and sweeps it before
creating another surface; surfaces are not submission-delayed.

**Internal access.** Each wrapper's trivial `backend()` accessor returns
`m_resource.access()` (the roots return their owned backend) and is defined in its
`src/acm*.cpp`, where the backend type is complete. It is the internal seam the interop
functions use, not a native-handle API. Slot IDs stay inside `ResourceRef` and
`ResourceSlot`. Backend classes collaborate through narrow, validated operations on the
typed slot they were given and never rediscover a resource from an index. Copying a
wrapper does not allocate, and recording does no map lookup or Device forwarding.

`bench/resource_pool.cpp` measures these costs: `resource_pool` (reference copy and
access, pool emplace) and `resource_pool_collect` (a sweep with nothing dead, and a release
plus sweep of one slot among 4k live ones).

## Ownership and the object graph

`acm::Instance` and `acm::Device` are unique, move-only owning roots. Each holds a
`unique_ptr` to its heap-stable backend, so moving a root never moves the backend its
children point at. A `vulkan::Device` borrows its `vulkan::Instance`. Dependent resources
retain the public wrappers they need: a `RenderTarget` its texture and owned depth texture,
a `Pipeline`, `ComputePipeline` or `DescriptorSet` its `DescriptorSetLayout`, a `SwapChain`
its `Surface`, a `Renderer` its `SwapChain` and `CommandPool`, a `CommandBuffer` its
`CommandPool` (and the target of an open rendering scope), and a `DescriptorSet` every
texture, sampler and buffer written into it. A `CommandBuffer` queues
`vkFreeCommandBuffers` before releasing its pool.

Arrows are the factories; the graph is a lifetime hierarchy, not shared ownership.

```
Instance ── devices() / graphicsOptions() / surfaceOptions() ──> DeviceInfo, DeviceOption, SurfaceOption
   ├── interop::adoptSurface(instance, VkSurfaceKHR) ──> Surface      // external platform surface
   ├── createHeadlessSurface([extent]) ───────────────> Surface      // windowless surface
   └── createDevice(DeviceOption[, DeviceConfig]) ────> Device       // logical device + queue + features
          ├── createSwapChain(Surface, SurfaceOption[, SwapChainConfig]) ──> SwapChain ── renderTarget(i)
          ├── createRenderer(SwapChain) ─────────────────> Renderer         // the frame loop
          ├── createShader(spirv) ───────────────────────> Shader
          ├── createPipeline(PipelineShaders, RenderTarget[, PipelineConfig]) ──> Pipeline
          ├── createComputePipeline(Shader, DescriptorSetLayout) ──> ComputePipeline
          ├── createCommandPool() ───────────────────────> CommandPool ── allocate() ──> CommandBuffer
          ├── createTexture(Format, Extent2D[, TextureConfig]) ──> Texture
          ├── createBuffer(size, BufferUsage) ──────────> Buffer
          ├── createRenderTarget(Texture[, RenderTargetConfig]) ──> RenderTarget   // offscreen
          ├── createSampler([maxAnisotropy]) ───────────> Sampler
          ├── createDescriptorSetLayout(bindings | samplerCount) ──> DescriptorSetLayout
          └── createDescriptorSet(DescriptorSetLayout) ──> DescriptorSet
```

`createSwapChain(Surface, SurfaceFormat, PresentMode[, SwapChainConfig])` is the form for a
caller that picked the format and present mode itself.

Inside the backend, `vulkan::Device` owns the pools and factories, the one queue and its
mutex, the submission serial, the deferred destroy queue and the memory allocator.
`vulkan::SwapChain` owns the `VkSwapchainKHR` and its per-image targets; `vulkan::Renderer`
owns per-frame synchronization and command buffers.

An invalid `acm::Device` (default-constructed, moved-from, reset, or failed creation) is
safe to call: factories return invalid children, capability queries return neutral values
(an empty `DeviceInfo`, no features, `SampleCount::One`, alignment 1, zero memory blocks),
`waitIdle()` does nothing and `submitSync` returns an error.

## Instance, devices and surfaces

**Vulkan 1.3 baseline.** The instance asks for API 1.3 and exposes only physical devices
whose `apiVersion` is at least 1.3 and which support the core `synchronization2` and
`dynamicRendering` features (an instance with none is invalid). Device creation enables
both. Rendering uses dynamic rendering, with no `VkRenderPass` or framebuffer; barriers use
`VkDependencyInfo` with the `*MemoryBarrier2` structures; every submission is a
`VkSubmitInfo2` through the device's one submission path. Properties come from the `*2`
queries: `Device` caches the general properties for limits and sample counts, the
allocator caches memory properties, and `Texture` queries format properties per format.

**InstanceConfig** ([acmInstance.h](../include/archimedes/acmInstance.h)) goes with the
application name and `acm::Version` ([acmVersion.h](../include/archimedes/acmVersion.h);
`acm::VERSION` is the engine's own): `portability` (default on, see CLAUDE.md),
`validation` (the Khronos layer, enabled as an instance layer only; device creation leaves
the legacy layer fields empty),
`debug` (`VK_EXT_debug_utils`, relaying debug-messenger messages to the log hook) and
`cpuDriver` (default on). A requested layer or extension the loader lacks is skipped with
a logged warning; `Instance::validationEnabled()` reports the outcome. Setting
`ACM_VALIDATION` to anything but empty or `0` turns `validation` and `debug` on for every
instance whatever its config says.

**The CPU driver is loaded directly.** With `cpuDriver` on and
`VK_LUNARG_direct_driver_loading` offered by the loader, `vulkan::Instance` loads the
staged CPU driver library from `<exe dir>/vulkan/` (`acm::executableDirectory()` in
[Platform.h](../private/archimedes/Platform.h), loaded by the move-only
`acm::PlatformLibrary` in [PlatformLibrary.h](../private/archimedes/PlatformLibrary.h)),
asks the driver for its instance extensions, and chains a `VkDirectDriverLoadingListLUNARG`
in inclusive mode (the loader's own drivers stay) onto `VkInstanceCreateInfo`. No
environment variable or registry entry is involved, so it works in an elevated Windows
process, where the loader ignores driver environment variables. The loader cannot list a
directly loaded driver before `vkCreateInstance`, so the driver's extensions (its surfaces)
are merged into the available list before surface extensions are chosen. The library stays
loaded until after `vkDestroyInstance`. MoltenVK stays on the loader's own discovery: GLFW
needs it before an instance exists, and MoltenVK 1.4.1 speaks loader-driver interface 5,
below direct loading's 7. The reasoning is in
[ADR-0001](adr/0001-cpu-device-via-staged-swiftshader.md).

**Device selection.** `Instance::graphicsOptions()` returns graphics-capable
`DeviceOption`s (`deviceIndex` + `queueFamily`) ranked by `DevicePreferences`, a ranked
list of `PhysicalDeviceType`s. `DevicePreferences::Default()` (used by the forms without
preferences) puts hardware devices first and the CPU device last;
`DevicePreferences::Cpu()` puts the CPU device first. The sort is stable, so devices of
one kind keep loader order, which is not defined between drivers. A CPU device and a
hardware device can coexist in one process; resources never cross devices, so moving data
between them is a host readback and an upload.
`Instance::surfaceOptions(surface[s][, SurfacePreferences[, DevicePreferences]])` filters
those options to queues that present to every given surface and adds the surface format,
present mode and capabilities `Device::createSwapChain(surface, option)` takes. Device kind
is the primary ranking; `SurfacePreferences` ranks format and present mode within a
device, and an empty `SurfacePreferences{}` keeps backend order on any unspecified axis.
`Instance::createDevice(option[, DeviceConfig])` builds the device.

**Surface color.** `SurfacePreferences::Default()` lists only non-encoding (UNORM) formats,
BGRA then RGBA in `SrgbNonlinear`, with FIFO first as the portable vsynced mode.
`HardwareSrgb()` lists SRGB formats instead; `LowLatency()` puts Mailbox and Immediate
ahead of FIFO. On a UNORM surface the driver stores what a shader writes without applying
the linear-to-sRGB transfer function, so each producer writes display-ready sRGB: a GUI
overlay emits its already-sRGB colours as they are, and linear-lit rendering encodes in its
shader before writing (see [mesh.frag](../testbed/shaders/mesh.frag)). Because only UNORM
formats are listed, a surface without one fails loudly instead of washing out; a consumer
that wants hardware encoding passes `HardwareSrgb()` or its own preferences.

**Device features.** `DeviceFeatures` ([acmDeviceInfo.h](../include/archimedes/acmDeviceInfo.h))
is the curated set of optional features the renderer uses: `fillModeNonSolid` (wireframe),
`wideLines`, `samplerAnisotropy` and `sampleRateShading`. Enumeration reports each physical
device's support in `DeviceInfo::features`. `DeviceConfig` carries `requiredFeatures` and
`optionalFeatures` (default `DeviceFeatures::AllKnown()`, enabling every supported curated
feature). Creation fails with an `acm::Error` when a required feature is missing, and
otherwise enables `supported & (required | optional)` through the
`VkDeviceCreateInfo::pNext` chain; `Device::enabledFeatures()` reports the result. A
consumer that did not require a feature degrades when it is off rather than producing an
invalid object, and logs a warning through the log hook: a pipeline falls back to fill
mode, line width 1 or no sample shading, and a sampler to isotropic filtering.

**Surfaces.** `acm::interop::adoptSurface(instance, VkSurfaceKHR)` wraps a surface created
by external code (the testbed's GLFW windows). Ownership of the `VkSurfaceKHR` moves to
Archimedes; the caller keeps the native window alive until the `acm::Surface` is reset.
`Instance::createHeadlessSurface(extent)` is the windowless surface that still exercises
the real swapchain and present path. It prefers `VK_EXT_headless_surface` (MoltenVK, and
the directly loaded SwiftShader) and otherwise creates an off-screen Win32 window that the
surface owns and destroys after its `VkSurfaceKHR`; it returns an invalid `Surface` where
neither exists. `extent` sizes that backing window and is unused on a true headless
surface, where the swapchain's own extent decides the render size, so pass the intended
size to both. Raw platform surface creation lives only in
[PlatformSurface.cpp](../src/vulkan/PlatformSurface.cpp). On Windows, where the extension
comes from SwiftShader, the hardware drivers do not support it and cannot present to the
headless surface, which matters for testing there (see [ROUGH-EDGES.md](ROUGH-EDGES.md)).

## Frame loop and swapchain

`Renderer::render([prePass,] record)` ([Renderer.cpp](../src/vulkan/Renderer.cpp)) drives
one frame:

1. Rebuild the swapchain if the last frame asked for it. `SwapChain::recreate()` returns
   `false` for a zero-extent (minimized) surface, and `render` then skips the frame and
   returns no error.
2. Wait the frame slot's fence if a submitted batch will signal it, then collect garbage up
   to that batch's serial.
3. Acquire an image. `VK_ERROR_OUT_OF_DATE_KHR` marks the swapchain for rebuild and skips
   the frame.
4. Record: `begin()`, then `prePass(cmd, frameIndex)` outside any rendering scope (compute,
   barriers, image transitions), then `beginRendering` on the acquired image's target,
   `setViewportAndScissor`, `record(cmd, frameIndex)`, `endRendering()` (closing a scope a
   callback left open) and `end()`.
5. Submit and present under the queue mutex. A suboptimal or out-of-date present, or a
   failed submit, marks the swapchain for rebuild.

`frameIndex` (0 to `Renderer::MaxFramesInFlight - 1`) names the slot whose fence was just
waited, so per-frame resources indexed by it (a ring of uniform buffers and their
descriptor sets) are safe to rewrite in the callbacks. Size such rings to
`acm::Renderer::MaxFramesInFlight`; there is no general per-frame resource abstraction.

**An acquired frame always completes.** Once an image is acquired, the frame is submitted
and the slot advances whatever recording does. If recording failed (a callback's call was
rejected, or the scope could not begin), the command buffer is re-recorded as a bare clear
of the target and presented, `render` returns the original error, and the next frame works
normally (`test_renderer.cpp`). If even the clear cannot be recorded, the device submits a
batch with no command buffer that consumes the acquire semaphore and signals the fence,
skips the present and marks the swapchain for rebuild, which releases the acquired image.
Fences start unsignalled and are waited only when a batch that signals them is pending.

**Synchronization objects.** Acquire semaphores and fences are per frame in flight.
Render-finished (present-wait) semaphores are per swapchain image, because no fence covers
a present's wait but acquiring the same image again does; they are rebuilt when the
swapchain's generation changes. Renderer teardown waits the device idle before destroying
them. A render target's begin barrier orders a swapchain image after the acquire wait (at
color-attachment output) and an offscreen texture after its own finish-state read
(`Sampled` or `CopySrc`); MSAA and depth images are ordered after their previous attachment
writes.

**Recreation.** `SwapChain::recreate()` ([SwapChain.cpp](../src/vulkan/SwapChain.cpp))
waits the device idle (`Device::waitIdle()`, under the queue mutex), rebuilds at the
surface's current extent (`currentExtent`, or `SwapChainConfig::extent` clamped to the
surface limits when the surface leaves it undefined) with the old swapchain as
`oldSwapchain`, then retires the old targets: each view of a borrowed image is destroyed
now and every copied `RenderTarget` wrapper goes stale, while the rest of each target (its
depth and MSAA images) goes through deferred destruction. Only then is the old swapchain
destroyed, and garbage is collected up to the serial the wait completed. The views go
first, and synchronously, because a sweep tags payloads with the newest serial, which
another renderer's submission can push past the wait. Recreating one swapchain is safe
while other renderers on the same device keep submitting. Pipelines use dynamic viewport
and scissor, so they survive a resize untouched.

## Threading and synchronization

The contract is in [CLAUDE.md](../CLAUDE.md#invariants); this is how the device meets it.

- One `VkQueue` per device. The device's queue mutex serializes `vkQueueSubmit2`,
  `vkQueuePresentKHR`, the `vkDeviceWaitIdle` in `waitIdle()`, the `vkQueueWaitIdle` in
  `submitSync` and the one-shot uploads, and `interop::withQueue` callbacks. Fence waits,
  acquires and command recording run unlocked, so renderers on different threads overlap
  everything but the submit.
- Each submit takes the next serial under the mutex. `Device::waitIdle()` waits the device,
  reads the last submitted serial under the same lock, then collects (outside the lock) up
  to that serial and returns it; nothing submitted after the wait can count as complete.
- No payload destructor runs while the queue mutex is held: `submitSync` and the one-shot
  uploads collect after unlocking. (Some destructors wait the device idle themselves, a
  `SwapChain`'s or a `Renderer`'s, and would deadlock.) For the same reason `waitIdle`,
  `submitSync` and `SwapChain::recreate` must not be called from inside `withQueue`.
- The deferred destroy queue, the memory allocator and each resource pool have their own
  mutex, never held while a payload is destroyed.
- `test/test_renderer_threads.cpp` runs two renderers on two threads over one device
  through swapchain recreation; the resource-pool stress test covers concurrent retain,
  release, access and collection. The ThreadSanitizer CI lane runs both, with the rest of
  the suite.

## Command recording

A `CommandBuffer` ([acmCommandBuffer.h](../include/archimedes/acmCommandBuffer.h)) comes
from `CommandPool::allocate()`, or already begun from `Renderer::render` and
`Device::submitSync(record)`.

**Errors.** Every recording call returns an `acm::Error`. A rejected call records nothing.
The first rejection since `begin()` is also kept as the sticky `error()` (first error wins,
so a run of unchecked calls can be checked once); it lives in the backend command buffer,
so every copy of the handle reports the same one, and `begin()` clears it. Calls are
rejected for:

- an invalid or stale argument, or one owned by another device;
- recording before `begin()` or after `end()`;
- copies, barriers, transitions and dispatches inside a rendering scope, and a second
  `beginRendering` while one is open;
- a draw without an active target and a compatible bound pipeline, or a pipeline whose
  target signature (color format, depth format, sample count) does not match the active
  target;
- a descriptor set whose layout is neither the pipeline's nor identical to it, a pipeline
  built without a descriptor layout, a dynamic-offset count that does not match the set's
  `UniformBufferDynamic` count, a misaligned or out-of-range offset, or a compute set with
  dynamic descriptors;
- a buffer bind, a readback copy or a compute descriptor bind whose resource has the wrong
  usage or layout (a vertex bind needs a `Vertex` buffer, a compute bind a set from the
  compute pipeline's layout);
- `dispatch` with no compute pipeline bound or past `maxComputeWorkGroupCount`, a readback
  buffer smaller than width × height × texel size, empty barrier stages, a transition to
  `ImageLayout::Undefined` or of a depth texture, a zero viewport extent.

`begin()` on a buffer left recording resets it first; `end()` closes a scope left open and
reports it. `Device::submitSync(record)` records into a transient command buffer, returns
the first recording error without submitting anything, and otherwise submits and waits the
queue idle under the queue mutex (safe while other threads submit, but it stalls them: a
load-time tool). `Device::submitSync(commandBuffer)` submits an ended buffer as recorded and
refuses one still recording; check `error()` first when partial work must not run. The
renderer handles errors as described under the frame loop.

**Recorded resources are not retained.** A command buffer holds no reference to the
buffers, textures, pipelines, descriptor sets or offscreen targets it records. Keep them
alive until the submission that uses them has been made, that is until `render()` or
`submitSync()` returns; after that, releasing them is safe, because deferred destruction
waits for the submission. A resource released before its submission (a per-draw buffer
created and dropped inside a render callback, say) can be destroyed by any sweep that runs
first: another thread's frame, or a `Buffer::write` or `submitSync` in the same callback.
The rule is written in the public headers; whether command buffers should retain is open
(see [WORK.md](../WORK.md)).

**Barriers.** `bufferBarrier(buffer, srcStage, dstStage)` is a conservative shader
read|write dependency between two non-empty `ShaderStage` sets, covering read-after-write,
write-after-read and write-after-write on a storage buffer, both within one command buffer
and across submissions in queue order. `transitionImage(texture, from, to)` is an image
barrier over a neutral `ImageLayout` (`Undefined`, `General`, `ShaderReadOnly`,
`TransferSrc`, `TransferDst`) that infers conservative access and stages. There is no
automatic layout tracking.

## Textures, buffers and memory

**Textures.** `acm::Texture` ([Texture.cpp](../src/vulkan/Texture.cpp)) is the one
owned-image type: image, device-local memory and a view. Swapchain images are borrowed
(freed by `vkDestroySwapchainKHR`), so a swapchain `RenderTarget` is built directly from
the borrowed `VkImage`. Usage follows the format: a color format gets
`COLOR_ATTACHMENT | SAMPLED | TRANSFER_SRC | TRANSFER_DST`, a depth format (`D32_Sfloat`,
`D24_Unorm_S8_Uint`) `DEPTH_STENCIL_ATTACHMENT` with a depth-aspect view.
`TextureConfig::storage` (color only) adds `STORAGE` for compute writes;
`TextureConfig::mipmapped` (color only) asks for a full chain of
`floor(log2(max(width, height))) + 1` levels. The level count is decided at creation: the
chain is generated by linear blits, so a format whose optimal tiling cannot blit and
linearly filter gets one level, with a logged warning (`mipLevels()` reports it); a depth
format asked for mipmaps gets one level and the same warning. A
mipmapped texture's view spans every level, so it is a sampling resource and cannot be a
`RenderTarget` attachment.

`Texture::upload(pixels, size)` stages tightly packed pixels and, in one synchronous
submit, transitions to `TRANSFER_DST`, copies, blits each further level from the one above
for a mipmapped texture, and leaves every level in `SHADER_READ_ONLY`. It returns an error
and submits nothing for a depth texture, null pixels, or a `size` other than
width × height × bytes per pixel.

**Buffers.** `acm::Buffer` ([Buffer.cpp](../src/vulkan/Buffer.cpp)) picks its heap by
`BufferUsage`: `Vertex` and `Index` are device-local; `Uniform`, `TransferDst`, `Staging`
and `Storage` are host-visible and coherent. Usages do not combine. `write(data, size)`
writes to the start of the buffer, a `memcpy` into the persistent mapping for host-visible
memory or a staging buffer plus one-shot copy for device-local memory; it returns an error,
writing nothing, for null data or a size larger than the buffer. `map()` and `unmap()` work
on host-visible buffers only. Geometry comes from buffers, not the shader: fill `Vertex` and
`Index` buffers with `write`, build the pipeline with a `PipelineConfig::vertexLayout`, and
record `bindVertexBuffer`, `bindIndexBuffer`, `drawIndexed`. Per-draw constants (a matrix, a
color) live in a `Uniform` buffer bound through a descriptor set. Both staging paths
(`Buffer::write` to device-local memory and `Texture::upload`) go through
`vulkan::Device::submitOneShot`, which waits the queue idle, so they are load-time calls.

**Memory.** `vulkan::MemoryAllocator` ([Memory.h](../private/archimedes/vulkan/Memory.h),
[Memory.cpp](../src/vulkan/Memory.cpp)), one per device, carves allocations out of 64 MB
`VkDeviceMemory` blocks with a per-block first-fit free list, coalesced on free, so
thousands of resources share a handful of allocations; a larger request gets its own
exactly sized block. Each block holds one memory type and one `AllocationKind`: `Linear`
for buffers, `NonLinear` (the default argument of `allocate`) for optimal-tiling images. A
buffer and an image therefore never share a block, which satisfies `bufferImageGranularity`
without padding, at the cost of one extra block per memory type used by both kinds.
Blocks of a host-visible memory type are persistently mapped, so `Buffer::map` returns a
pointer computed from the block. Mapping follows the chosen type, not the request: on
unified memory (the CPU device, some integrated GPUs) one type is both device-local and
host-visible, and a block a device-local request created is reused by host-visible ones.
`allocate`, `free` and `blockCount` lock the allocator's mutex. Resources enqueue their
`free` on the deferred queue; blocks are freed when the device is destroyed, after that
queue is flushed. `Device::memoryBlockCount()` is the one public diagnostic.

## Render targets, depth and MSAA

A `RenderTarget` records dynamic-rendering metadata: color and depth formats for pipeline
creation, image views for `vkCmdBeginRendering`, explicit synchronization2 layout
transitions and the final color layout. Pipeline creation takes a `RenderTarget`, which
keeps formats, views and layouts out of the public headers.

**Offscreen targets.** `Device::createRenderTarget(texture, RenderTargetConfig)` renders
into a single-level color `Texture`. `RenderTargetConfig::finish` sets the final layout so
no manual barrier is needed: `Sampled` leaves it `SHADER_READ_ONLY` for a later pass,
`CopySrc` leaves it `TRANSFER_SRC` for `CommandBuffer::copyTextureToBuffer`.

**Depth.** `RenderTargetConfig::depth` or `SwapChainConfig::depth` gives the target an owned
`D32_Sfloat` depth texture (each swapchain image gets its own), cleared to 1.0 by
`beginRendering` and not stored. A pipeline that tests or writes depth
(`PipelineConfig::depth = DepthState::Test(...)` or `TestWrite(...)`) needs a depth target
and is rejected otherwise; depth write without depth test is rejected. Every graphics
pipeline carries a depth-stencil state (disabled for `DepthState::None`), so a pipeline
without depth testing renders into a depth-bearing target (`test_depth.cpp`).

**MSAA.** `RenderTargetConfig::samples` or `SwapChainConfig::samples` (`SampleCount`) is
clamped to `Device::maxSampleCount()`, with a logged warning when clamped. With more than
one sample the target owns a multisampled color image (and, with depth, a multisampled
depth image) that rendering resolves into the single-sampled target. The pipeline takes the
resolved sample count from its target, so the two cannot disagree.
`PipelineConfig::minSampleShading` above 0 (needing `sampleRateShading` and a multisampled
target) shades per sample, smoothing aliasing inside primitives as well as at edges.

## Pipelines

`Device::createPipeline(PipelineShaders, RenderTarget[, PipelineConfig])`
([acmPipeline.h](../include/archimedes/acmPipeline.h)). `PipelineShaders` holds the
required `vertex` and `fragment` shaders; its `geometry` slot is rejected until the backend
supports it. `PipelineConfig` holds the optional state so independent knobs do not become
overloads: `vertexLayout` (a `stride` plus `VertexAttribute`s for vertex buffer binding 0;
empty means geometry comes from the shader), `descriptorLayout`, `depth`, `topology`,
`cullMode`, `frontFace`, `blend` (`Opaque` or `AlphaBlend`), `polygonMode`, `lineWidth` and
`minSampleShading`. A default-constructed config is the original smoke-test state
(triangle list, no culling, clockwise front face, opaque, fill, width 1, no sample
shading, no depth), which `PipelineConfig::Default()` names; `Mesh3D()`, `Sprite2D()` and
`Wireframe(width)` are opt-in starting points to customize field by field.
`polygonMode`, `lineWidth` and `minSampleShading` depend on enabled features and fall back
as described under device features. Viewport and scissor are dynamic state.

The backend `Pipeline` keeps its target's signature, which `bindPipeline` and the draws
check against the active target, so an incompatible pair fails before Vulkan validation
sees it.

## Descriptor sets

A `DescriptorSetLayout` is a list of `DescriptorBinding`s: binding index, `DescriptorType`
(`UniformBuffer`, `UniformBufferDynamic`, `StorageBuffer`, `CombinedImageSampler`,
`StorageImage`, `SampledImage`), `ShaderStage` flags (`Vertex`, `Fragment`, `Compute`, or a
combination such as `Vertex | Fragment`) and `count` (above 1 makes a descriptor array).
`createDescriptorSetLayout(n)` is the shorthand for n fragment-stage samplers at bindings
0 to n-1. A `DescriptorSet` owns its own one-set pool, sized from its layout. Build the
pipeline with `PipelineConfig::descriptorLayout` and record `bindPipeline`, then
`bindDescriptorSet(pipeline, set)`, then the draw; the set binds as set 0.

**Writes** ([acmDescriptorSet.h](../include/archimedes/acmDescriptorSet.h)):
`setTexture(binding, texture, sampler[, element])`, `setBuffer(binding, buffer[, element])`
(uniform or storage, matching the binding), `setDynamicBuffer(binding, buffer, elementSize)`,
`setStorageImage(binding, texture)` and `setSampledImage(binding, texture)`. Each returns an
`acm::Error` and changes nothing when the set is invalid, the binding is not in the layout,
the element is past the binding's count, the binding's type does not match the call, or a
resource is invalid, owned by another device or unusable there: a depth texture, a buffer
past the device's uniform or storage range limit, a zero element size, a buffer whose usage
does not fit the descriptor, or a storage-image texture created without `storage`. A
successful write retains the texture, sampler or buffer until that element is rewritten or
the set is destroyed, so callers may drop their wrappers once written
(`test_descriptors.cpp`). Copies of a set share one native set; writing it is
caller-synchronized and must not happen while a submitted command still uses it.

**Dynamic uniform buffers.** A `UniformBufferDynamic` binding packs many objects' constants
into one buffer, with the per-draw base offset supplied at bind time, so one buffer and one
set serve many draws. `setDynamicBuffer` sizes the descriptor to one element;
`bindDescriptorSet(pipeline, set, dynamicOffset)` takes the slice's byte offset, which must
be a multiple of `Device::minUniformBufferOffsetAlignment()` and, with the element size,
stay inside the buffer.

**Storage images.** A `StorageImage` binding is a compute target: a `storage` texture a
shader writes with `imageStore`, bound in `GENERAL` layout. Transition it to `General`
before the dispatch and to `TransferSrc` or `ShaderReadOnly` to copy it out or sample it.

**Sampled images.** A `SampledImage` binding is a read-only image with no sampler, in
`SHADER_READ_ONLY`, for a shader that takes its sampler from elsewhere or reads texels with
`texelFetch`. It is also how an external renderer with a separate sampler set gets a
descriptor Archimedes owns: lain's GUI allocates one against a layout identical to ImGui's
texture set and gives ImGui `acm::interop::descriptorSet(set)` as its texture id, so
releasing it defers destruction past the frames that drew it, which ImGui's own
`RemoveTexture` (an immediate free) does not.

**Sampling.** `Device::createSampler([maxAnisotropy])` is linear, clamp-to-edge and
trilinear (linear mipmap mode over the full level range, a no-op on single-level textures).
`maxAnisotropy` above 1 enables anisotropic filtering, clamped to the device limit and
dependent on `samplerAnisotropy`.

**Uniforms that change every frame** cannot live in one buffer, because the GPU may still
read frame N while the CPU writes frame N+1. Keep one buffer and descriptor set per frame
in flight and select them with the renderer's `frameIndex`
(`testbed/src/examples/ComputeTextureExample.cpp` does this for its time uniform).

## Compute

`Device::createComputePipeline(shader, layout)` builds a `ComputePipeline` from one compute
`Shader` and a pipeline layout made from a `DescriptorSetLayout`, with no rendering scope
or fixed-function state. Record `bindComputePipeline`, `bindComputeDescriptorSet` (a set
from that layout, with no dynamic descriptors) and `dispatch(gx, gy, gz)` outside any
rendering scope. The resources it reads and writes are ordinary descriptors with
`ShaderStage::Compute`, typically a host-visible `BufferUsage::Storage` buffer or a storage
image. One-shot compute runs through `Device::submitSync(record)`. Per-frame compute that
feeds the draws goes in the renderer's `prePass`, in the same command buffer as the draws
with a `bufferBarrier` or `transitionImage` between them, so it needs no extra submit
(`test_renderer.cpp`). Across separate submissions, barriers order work in queue order:
the testbed brackets its mesh deformation with `bufferBarrier(Vertex, Compute)` before and
`bufferBarrier(Compute, Vertex)` after, instead of waiting the device idle each frame.

## Diagnostics

**The log hook.** [acmLog.h](../include/archimedes/acmLog.h) is one process-wide handler
for every message Archimedes produces, with no logging dependency. A `LogMessage` carries a
`LogSeverity` (`Verbose`, `Info`, `Warning`, `Error`, ordered), a `LogCategory` (`Library`,
`Vulkan`, `Validation`, `Performance`), an `id` (a validation message's VUID name, or
empty) and the `text`. `setLogHandler(handler)` installs a handler and returns the one it
replaced; an empty handler discards everything. The handler starts as `defaultLogHandler`,
which writes warnings and errors to stderr, one line each. Delivery is synchronous, on the
thread that produced the message, so a handler must be thread-safe; one that throws has the
exception swallowed. Library code reports through the private
[Log.h](../private/archimedes/Log.h) helpers (`logWarning` and friends): one self-contained
line naming the object, Warning for a degraded or ignored request, Info for a notable
expected event, Verbose for debugging detail, Error for a failure alongside the returned
`acm::Error`. The library warns when a requested validation layer or debug extension is
missing and for every feature, sample-count and mip-chain fallback described above.

**Validation.** With `InstanceConfig::debug` (or `ACM_VALIDATION`), every debug-messenger
message reaches the hook with its own severity: validation reports as `Validation`,
performance warnings as `Performance`, loader and driver messages as `Vulkan`. Validation
messages reach the hook only with `debug` on as well as `validation`, which
`ACM_VALIDATION` does. How the test suite uses this is in
[TESTING.md](TESTING.md#validation-and-sanitizers).
