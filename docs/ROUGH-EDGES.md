# Known rough edges

Limitations that are true of the code today. What we might do about them, in priority
order, is in [WORK.md](../WORK.md). Remove an entry in the change that fixes it.

## Frame loop and synchronization

- **Command buffers do not retain recorded resources.** A buffer, texture, pipeline,
  descriptor set or offscreen target released before the submission that uses it can be
  destroyed by a sweep that runs first (another thread's frame, or a `Buffer::write` or
  `submitSync` in the same callback). The public headers state the rule: keep what you
  record alive until `render()` or `submitSync()` returns. Whether command buffers should
  retain instead is an open design question.
- **Present semaphores and device idle.** The renderer destroys its present-wait semaphores
  after a device-idle wait, assuming the wait covers the presents that waited on them. The
  spec guarantees that only with `VK_EXT_swapchain_maintenance1`, which is not used.
- **A lost device can leave an acquire semaphore signalled.** If both a frame's submit and
  its fallback empty submit fail (in practice, a lost device), nothing consumes that frame
  slot's acquire semaphore.
- **Recreation is driven only by return codes.** The swapchain is rebuilt on
  `VK_ERROR_OUT_OF_DATE_KHR` or `VK_SUBOPTIMAL_KHR` (reliable on MoltenVK). There is no
  resize-flag fallback for a driver that never reports out-of-date on resize.
- **One queue.** Every renderer on a device submits through one queue and its mutex; there
  is no per-renderer queue and no async compute queue.

## Resources

- **Texture formats are not checked against their usage.** `Texture` creation does not
  check that the format supports the usage it requests with optimal tiling (for example
  `R32G32B32_Sfloat` as a color attachment).
- **Mipmaps are generate-on-upload only.** Levels come from a linear blit-down chain (a box
  filter, not a Kaiser or sRGB-correct downsample); the texture must be created
  `mipmapped`, with no deferred generation or regeneration; a format that cannot be blitted
  and linearly filtered gets one level; a mipmapped texture cannot be a `RenderTarget`; there
  are no partial or per-level uploads.
- **Buffers have one purpose each.** `BufferUsage` has no combined flags and each usage's
  heap is fixed: vertex and index buffers are always device-local (no host-visible or
  per-frame vertex path), uniform and readback buffers always host-visible, and
  `BufferUsage::Storage` is host-visible only (no device-local storage). Index buffers are
  32-bit only.
- **The memory allocator is simple.** Per-block first-fit (no best-fit, buddy or
  defragmentation; allocation is O(regions)), a fixed 64 MB block size, and no shrinking:
  blocks live until the device dies.
- **Uploads stall.** `Buffer::write` to device-local memory and `Texture::upload` each create
  a throwaway staging buffer and command pool and wait the queue idle: load-time tools, not
  per-frame ones.

## Pipelines, depth and descriptors

- **Fixed-function state is partial.** `BlendMode` is a two-way preset (opaque or
  src-alpha over), not arbitrary factors, and there is no depth bias or stencil state.
- **Device features are a curated bool set** (`fillModeNonSolid`, `wideLines`,
  `samplerAnisotropy`, `sampleRateShading`, `fragmentStoresAndAtomics`). Extension and
  limit-style capabilities may want an enum or request model once they become part of
  negotiation. Shader-dependent features are not checked against the SPIR-V: a fragment
  shader that writes storage without `fragmentStoresAndAtomics`, or a vertex shader that
  does (there is no `vertexPipelineStoresAndAtomics`), is caught only by validation.
- **Depth is shallow.** The format is always `D32_Sfloat` (no stencil, no capability query
  or fallback), `DepthState` covers only test, write and compare, and the depth buffer is
  cleared and discarded (`storeOp` `DONT_CARE`), so it cannot be sampled or read back.
- **Descriptors.** No input attachments. Storage images are color-only, used in `GENERAL`,
  with layout transitions recorded by hand (`CommandBuffer::transitionImage`; no automatic
  tracking). Each `DescriptorSet` owns a one-set pool (no shared pool). Per-frame buffers
  and descriptor sets are managed by callers through the renderer's frame index; there is
  no general per-frame resource abstraction.

## CPU device and platforms

- **GLFW cannot see a directly loaded CPU driver.** GLFW checks the loader's instance
  extensions before any instance exists, where a directly loaded driver is invisible. On a
  machine whose only driver is the CPU driver and whose loader ignores driver environment
  variables (an elevated Windows process with no GPU driver), `glfwCreateWindowSurface`
  therefore fails; headless rendering, offscreen rendering and compute still work on the
  CPU device. Elsewhere a windowed CPU device works, because a driver the loader discovers
  itself supplies the surface extensions GLFW checks for.
- **The CPU driver needs a loader with `VK_LUNARG_direct_driver_loading`** (Vulkan SDK
  1.3.230 or newer). The vendored loader has it; a consumer supplying an older
  `Vulkan::Loader` gets no CPU device.
- **A windowed CPU device on macOS still needs Metal** (SwiftShader presents through a
  `CAMetalLayer`); headless rendering and compute do not. SwiftShader, like MoltenVK, lacks
  `wideLines`.

## Testing and CI

- **On Windows the hardware device skips every swapchain-backed test.**
  `createHeadlessSurface` prefers `VK_EXT_headless_surface`. Since the CPU device work, the
  directly loaded SwiftShader supplies that extension, so whenever the CPU driver is loaded
  the Win32 off-screen window fallback goes unused, and the Windows hardware drivers, which
  lack the extension, cannot present to the resulting surface. The
  frame-loop, swapchain and two-thread renderer tests therefore run only on the CPU device
  there, and the testbed covers the hardware path.
- **The validation and ThreadSanitizer CI lanes have never run.** A machine without a
  validation layer runs the suite unvalidated (unless `ARCHIMEDES_TEST_VALIDATION=REQUIRE`),
  benchmarks are validated only with `ACM_VALIDATION=1`, and the synchronization-validation
  pass is informational.
- **Warnings are errors only in the Windows CI lane**, until the Linux and macOS lanes have
  had a clean run with `ARCHIMEDES_WARNINGS_AS_ERRORS=ON`.
- **Sanitizers need Clang or GCC**; `ARCHIMEDES_SANITIZE` is refused with MSVC.
