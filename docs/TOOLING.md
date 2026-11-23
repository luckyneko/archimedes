# Tooling: build, dependencies and CI

The everyday commands are in [CLAUDE.md](../CLAUDE.md#build-test-run); this file explains
what the build fetches, how the Vulkan runtime is staged, every CMake option, and what CI
runs.

## Dependencies

Nothing is installed system-wide. Each module under [cmake/](../cmake/) prefers a system
package where that makes sense and otherwise fetches a pinned release with
`FetchContent`. Archives are cached under `.cache/fetch/` in the top-level source
directory (`CMAKE_SOURCE_DIR`, so a parent project gets its own cache), at a path that
mirrors the source URL, set per `FetchContent_Declare` through `DOWNLOAD_DIR`. Several
archives share a leaf name (Vulkan-Headers, Vulkan-Loader and glslang are all
`vulkan-sdk-<version>.tar.gz`), and the mirrored path keeps them apart. Sources are
extracted into `build/_deps/`, so deleting `build/` re-extracts from the cache without a
download. Both directories are git-ignored.

| Dependency | Module | Needed by |
|---|---|---|
| Vulkan-Headers | [acmVulkan.cmake](../cmake/acmVulkan.cmake), `acm_require_vulkan_headers()` | the library |
| Vulkan-Loader | `acm_require_vulkan_runtime()` | tests, benchmarks, testbed |
| MoltenVK (macOS) | `acm_require_vulkan_runtime()` | runnable targets on macOS |
| SwiftShader, the CPU driver | `acm_require_vulkan_runtime()` | runnable targets |
| GLFW | [addGLFW.cmake](../cmake/addGLFW.cmake) | testbed |
| glslang | [addGlslang.cmake](../cmake/addGlslang.cmake) | testbed shaders |
| Catch2 | [addcatch2.cmake](../cmake/addcatch2.cmake) | tests, benchmarks |
| clang-format 20 | [addclangformat.cmake](../cmake/addclangformat.cmake) | `format` / `format-check` |

There is no logging dependency: diagnostics go to each instance's own log handler
([ARCHITECTURE.md](ARCHITECTURE.md#diagnostics)). A library-only build
(`-DARCHIMEDES_BUILD_TESTBED=OFF -DARCHIMEDES_BUILD_TESTING=OFF -DARCHIMEDES_BUILD_BENCHMARK=OFF`)
fetches only Vulkan-Headers (unless an SDK is found), plus clang-format when
`ARCHIMEDES_FORMAT` is on.

### Why the library links only the Vulkan headers

Vulkan is several components. A static library needs only the headers to compile; the
loader, the drivers and the validation layers are link- and run-time concerns of an
executable. So `archimedes` links `Vulkan::Headers` (include-only) and leaves its `vk*`
symbols unresolved, and every runnable target (tests, benchmarks, testbed, a consumer's
application) links `Vulkan::Loader`.

### glslang and shaders

`addGlslang.cmake` prefers a system `glslc` or `glslangValidator` and otherwise builds
glslang's standalone compiler from source (`ENABLE_OPT=OFF`, so no SPIRV-Tools). The choice
is re-made on every configure, so it is never stale-cached. It defines
`vk_target_shaders(<target> <sources...>)`, which compiles GLSL for `--target-env
vulkan1.3` to `<bindir>/shaders/*.spv`. The tests do not use it: their SPIR-V is embedded
(see [TESTING.md](TESTING.md#the-test-suite)).

## The Vulkan runtime

Vulkan is the one dependency Archimedes passes through to consumers, through
`acm_`-prefixed functions in [acmVulkan.cmake](../cmake/acmVulkan.cmake) that a parent
project can call after `add_subdirectory` without touching Archimedes' module path:

- `acm_require_vulkan_headers()`: prefers an SDK found by `find_package(Vulkan)`, otherwise
  vendors Vulkan-Headers, and provides `Vulkan::Headers`. The library calls it itself.
- `acm_require_vulkan_runtime()`: builds Vulkan-Loader from source against those headers
  (`Vulkan::Loader`; its Apple-only `vulkan-framework` target is `EXCLUDE_FROM_ALL`), fetches
  the prebuilt MoltenVK on macOS, and provides the CPU driver. Every runnable target that
  creates an instance needs it.
- `acm_stage_vulkan_runtime(<target>)`: copies every staged driver (manifest, library and
  licence texts) to `<target dir>/vulkan/` after each build and, off Windows, generates
  `build/run_<target>.sh`. It does nothing when there is no staged driver.
- `acm_vulkan_driver_environment(<out>)`: the one `NAME=VALUE` loader environment entry
  that exposes the staged MoltenVK where it was provisioned (`VK_ICD_FILENAMES`, used by
  ctest); empty off macOS.

Each function is idempotent (it returns early if its target already exists, so a consumer
can supply its own Vulkan) and is version-locked to the single `ARCHIMEDES_VULKAN_SDK` pin
that headers, loader and glslang share.

**MoltenVK** is the prebuilt `MoltenVK-macos.tar` release (`ARCHIMEDES_MOLTENVK_VERSION`):
`libMoltenVK.dylib` plus `MoltenVK_icd.json`, which names the library relatively and marks
it a portability driver. It is Apache-2.0; `ACM_MOLTENVK_LICENSES` names its licence,
staged as `vulkan/MoltenVK.LICENSE`. The loader finds it through `VK_ICD_FILENAMES`.

**SwiftShader** (the CPU driver, [ADR-0001](adr/0001-cpu-device-via-staged-swiftshader.md))
is staged when `ARCHIMEDES_SOFTWARE_VULKAN` is on (the default). It comes from a pinned,
hash-checked `luckyneko/swiftshader-prebuilt` archive per host (linux-x64, macos-arm64,
windows-x64) holding `vk_swiftshader_icd.json`, the library it names, and SwiftShader's
`LICENSE.txt` (Apache-2.0) and `THIRD_PARTY_LICENSES.txt`. The pin (release tag plus one
SHA256 per platform) is set inside `_acm_require_cpu_driver()` as plain variables, so a
build folder's stale cache cannot hold an older tag, and so a parent project calling
`acm_require_vulkan_runtime()` sees it (a function sees its caller's variables, not
Archimedes' directory scope). The prebuilt is upstream SwiftShader with one patch:
`SWIFTSHADER_THREAD_COUNT` (1 to 256) sizes its worker pool. A host with no published
archive warns and stages nothing, so a default configure never breaks.
`ARCHIMEDES_SWIFTSHADER_DIR=<dir>` stages a local SwiftShader build instead (its CMake build
leaves both files in `build/<Darwin|Linux|Windows>/`, but no licence texts). The module
sets `ACM_SWIFTSHADER_ICD`, `ACM_SWIFTSHADER_LIB` and `ACM_SWIFTSHADER_LICENSES` (for a
consumer's own notices); staging copies the licences beside the library as
`vk_swiftshader.LICENSE.txt` and `vk_swiftshader.THIRD_PARTY_LICENSES.txt`. `acm::Instance`
loads the driver directly ([ARCHITECTURE.md](ARCHITECTURE.md#instance-devices-and-surfaces)),
so it needs no environment.

**Launchers and direct launches.** `run_<target>.sh` exports `VK_ICD_FILENAMES` for a
staged MoltenVK, plus `VK_LAYER_PATH` only if `VULKAN_SDK` was set at configure time
(macOS gets validation layers only from the LunarG SDK), then runs the binary. Direct
launches (an IDE, or Windows, which has no launcher) also work: the test, benchmark and
testbed executables call `acm::useStagedVulkanRuntime()`
([acmRuntime.h](../include/archimedes/acmRuntime.h)) before creating an instance. On macOS it
points `VK_ICD_FILENAMES` at the MoltenVK in `<exe dir>/vulkan/` and defaults
`MVK_CONFIG_LOG_LEVEL` to the `ARCHIMEDES_MOLTENVK_LOG_LEVEL` cache value (`1`, errors only,
which keeps MoltenVK's info-level startup logs out of test output). On every platform, with
`RuntimeConfig{cpuDeviceThreads}` it also sets `SWIFTSHADER_THREAD_COUNT`, which the driver
reads once at the first CPU device, so a host with its own process-wide pool (lain) can
split the machine with it. It never overwrites a variable the caller already set.

The testbed binds GLFW to the vendored loader with `glfwInitVulkanLoader(vkGetInstanceProcAddr)`,
so GLFW never loads a system `libvulkan`.

CI's `add_subdirectory` smoke ([.github/subproject-smoke](../.github/subproject-smoke))
provisions and stages the runtime from a parent project's scope and runs a binary that must
find the CPU device.

## CMake options

| Option | Default | Effect |
|---|---|---|
| `CMAKE_BUILD_TYPE` | `Release` | build type when none is given |
| `ARCHIMEDES_BUILD_TESTBED` | ON at top level | the GLFW testbed |
| `ARCHIMEDES_BUILD_TESTING` | ON at top level | the Catch2 suite |
| `ARCHIMEDES_BUILD_BENCHMARK` | ON at top level | `bench-archimedes` |
| `ARCHIMEDES_FORMAT` | ON at top level | the `format` / `format-check` targets |
| `ARCHIMEDES_CLANG_FORMAT_VERSION` | `20` | pinned clang-format major version |
| `ARCHIMEDES_CLANG_FORMAT` | unset | use this clang-format binary instead |
| `ARCHIMEDES_VULKAN_SDK` | `1.4.341.0` | Vulkan SDK tag for headers, loader and glslang |
| `ARCHIMEDES_MOLTENVK_VERSION` | `1.4.1` | MoltenVK release |
| `ARCHIMEDES_MOLTENVK_LOG_LEVEL` | `1` | default `MVK_CONFIG_LOG_LEVEL` for Archimedes-run executables |
| `ARCHIMEDES_SOFTWARE_VULKAN` | ON | stage the SwiftShader CPU driver |
| `ARCHIMEDES_SWIFTSHADER_DIR` | empty | stage a local SwiftShader build instead |
| `ARCHIMEDES_TEST_VALIDATION` | `AUTO` | validation in the tests: `AUTO`, `REQUIRE`, `OFF` |
| `ARCHIMEDES_TEST_SYNC_VALIDATION` | OFF | also turn on the layer's synchronization validation in the tests |
| `ARCHIMEDES_SANITIZE` | empty | sanitizers for the library, tests and benchmarks |
| `ARCHIMEDES_WARNINGS_AS_ERRORS` | ON top-level, OFF as a subproject | treat compiler warnings in Archimedes' own targets as errors |

The build is out-of-source only; the top-level `CMakeLists.txt` refuses an in-source
configure. Validation layers and the debug messenger are runtime choices
(`InstanceConfig`, `ACM_VALIDATION`), independent of the build type.

**Compiler warnings.** [acmWarnings.cmake](../cmake/acmWarnings.cmake) provides
`acm_warnings(<target>)`, applied to the library, the tests, the benchmarks and the testbed
but never to vendored dependencies: `/W4` on MSVC, `-Wall -Wextra` on GCC and Clang, plus
`/WX` or `-Werror` with `ARCHIMEDES_WARNINGS_AS_ERRORS`. That option is on by default for a
top-level build, locally and in every CI lane, and off when Archimedes is a subproject, so a
consumer with a different compiler is not broken by a warning in our code. The options are
`PRIVATE`, so a consumer's own sources keep their own flags.

**Sanitizers.** [acmSanitize.cmake](../cmake/acmSanitize.cmake) applies
`ARCHIMEDES_SANITIZE` (a comma list of `thread`, `address`, `undefined`, `leak`; Clang or GCC
only; `thread` combines with neither `address` nor `leak`) to Archimedes' own targets
through `acm_sanitize(<target>)`. Undefined behaviour aborts, so a test that hits it fails.

## CI

[.github/workflows/ci.yml](../.github/workflows/ci.yml) runs on pushes to `main`, `master` and
`develop`, on pull requests, and by hand:

- **clang-format**: `format-check` with the pinned clang-format.
- **Build matrix**: Linux Release, Linux Debug (Ubuntu 24.04), macOS Release (macOS 15) and
  Windows Release (Windows Server 2022, MSVC). Each builds the library alone, then the full
  graph, then runs `ctest`. The runners have no GPU, so the `[gpu]` tests run on the staged
  CPU device. Every lane builds with warnings as errors, the top-level default.
- **Linux validation**: fetches the prebuilt Khronos validation layer from the LunarG SDK
  release matching `ARCHIMEDES_VULKAN_SDK` (checked against a SHA-256 pinned in the workflow,
  which must be updated with the SDK pin; cached), configures with
  `ARCHIMEDES_TEST_VALIDATION=REQUIRE`, and runs the suite;
  a second, informational pass adds synchronization validation and never fails the job.
- **Linux TSan**: Clang, RelWithDebInfo, `ARCHIMEDES_SANITIZE=thread`, no testbed, the suite
  on the CPU device with `SWIFTSHADER_THREAD_COUNT=4`.
- **add_subdirectory smoke**: builds `.github/subproject-smoke` as a parent project and runs
  its binary on the staged CPU driver, checking the driver and its licences were staged.

Hardware-GPU coverage comes from local runs. What has not run yet is listed in
[ROUGH-EDGES.md](ROUGH-EDGES.md).
