# A CPU device comes from a staged SwiftShader driver, not a CPU backend

---
Status: accepted
---

Archimedes needs to render and run compute where no GPU driver works: GPU-less CI runners, sandboxed
processes without Metal access, fresh containers, and end-user machines. It also needs a CPU pathway
that a caller can choose on purpose, so one compute shader runs on a GPU or on the CPU. We get this by
staging **SwiftShader**, a CPU implementation of Vulkan 1.3, beside every runnable binary, exactly as
MoltenVK is staged today. The Vulkan loader then lists a **CPU device** (`PhysicalDeviceType::Cpu`)
beside any hardware device, and the choice between them is made at runtime through device preferences.
The library's one compile-time backend stays Vulkan.

## Considered options

- **A hand-written CPU backend (`acm::cpu`).** Rejected. Backend selection must stay compile-time (no
  virtual dispatch or runtime switching), so it could never be chosen at runtime. It would also mean
  running SPIR-V on the CPU ourselves, which is a driver's job.
- **lavapipe (Mesa).** Conformant Vulkan 1.3 and well maintained on Linux and Windows, but its macOS build
  is unresolved. macOS is the main development platform and the one whose sandboxes lack Metal. Using
  lavapipe on two platforms and SwiftShader on the third would mean two drivers with two sets of limits.
- **Bring your own driver.** Rejected. It breaks the "no system install" rule and leaves macOS uncovered.

## Decisions that follow

- **One CPU driver everywhere.** SwiftShader on linux-x64, macos-arm64 and windows-x64. Its upstream
  activity has slowed, so we pin it.
- **A pinned prebuilt.** A sibling repo, `luckyneko/swiftshader-prebuilt`, publishes hash-checked
  archives, following lain's FFmpeg and OpenCV prebuilts. The driver is a C-ABI shared library the
  loader loads, so one Release build per platform serves Debug and Release consumers alike.
- **Staged by default.** `ARCHIMEDES_SOFTWARE_VULKAN` (default ON) stages the CPU driver beside every
  runnable target. On a host with no published archive, configure warns and behaves as if the option
  were OFF. A library-only build never fetches it.
- **Loaded directly, not through the environment.** `acm::Instance` loads the staged CPU driver
  library itself and hands its entry point to the loader through `VK_LUNARG_direct_driver_loading`
  (inclusive mode, so system GPU drivers stay visible). Driver environment variables were the first
  approach, but the loader ignores them in an elevated Windows process (GitHub's Windows runners
  are elevated, which is how it surfaced). The loader cannot list a directly loaded driver's
  extensions before an instance exists, so `acm::Instance` asks the driver itself. MoltenVK stays on
  `VK_ICD_FILENAMES`: GLFW needs it before an instance exists, and MoltenVK 1.4.1 cannot be loaded
  directly (loader-driver interface 5, below the required 7).
- **Ranked selection.** `DevicePreferences::Default()` ranks hardware devices first and the CPU device
  last, so a caller taking the first option gets a GPU when one exists. `DevicePreferences::Cpu()` puts
  the CPU device first.
- **Coexistence.** A CPU device and a hardware device may exist in one process. Resources never cross
  devices; moving data between them is an explicit host readback plus upload.

## Consequences

- `[gpu]` tests run once for each device kind present. With the CPU driver staged, a missing CPU
  device is a test failure rather than a SKIP, because it means staging is broken. Capability gaps
  still SKIP.
- GLFW checks instance extensions before an instance exists, where a directly loaded driver is
  invisible. So on a machine whose only driver is the CPU driver and where the loader ignores driver
  environment variables (an elevated Windows process with no GPU driver), GLFW cannot create a
  window surface; headless rendering, offscreen rendering and compute still work.
- On macOS, SwiftShader presents to a window through a `CAMetalLayer`, so a windowed CPU device still
  needs Metal. Headless rendering and compute do not.
- SwiftShader runs shaders on its own thread pool, sized to the machine (up to 16 threads). A
  consumer with its own process-wide pool (lain) would oversubscribe the CPU. Upstream reads a
  thread count only from a `SwiftShader.ini` in the working directory, which a library cannot
  rely on, so the prebuilt carries one patch: `SWIFTSHADER_THREAD_COUNT` (1–256) sizes the pool.
  `acm::RuntimeConfig::cpuDeviceThreads` sets it, keeping the cap backend-neutral.
- Staging SwiftShader exposed a latent allocator bug: memory blocks were mapped only when the
  request that created them asked for host-visible memory, but on unified memory one type is
  both device-local and host-visible and blocks are shared. Blocks are now mapped by type.
