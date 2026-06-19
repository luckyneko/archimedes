# Provides a Vulkan target for archimedes to build against.
#
# "Vulkan" is several separate things. archimedes is a STATIC library, so it
# only needs the Vulkan *headers* to compile — the loader, the MoltenVK ICD,
# and the validation layers are link/run-time concerns of an eventual
# executable, not of this archive. We therefore expose only the include-only
# Vulkan::Headers target here.
#
# Prefers a real Vulkan SDK (find_package(Vulkan), which gives Vulkan::Headers
# for free); otherwise fetches Vulkan-Headers via FetchContent. The download
# path mirrors the source URL under .cache/fetch/ so similarly named archives
# across deps can't collide; sources extract into build/_deps.

include(FetchContent)

# Pinned to a Vulkan SDK tag; override with -DVULKAN_HEADERS_VER=...
set(VULKAN_HEADERS_VER "1.4.341.0" CACHE STRING "Vendored Vulkan-Headers SDK version")
set(VULKAN_HEADERS_FILE "github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/vulkan-sdk-${VULKAN_HEADERS_VER}.tar.gz")

# FIND_PACKAGE_ARGS NAMES Vulkan: a present SDK satisfies this and Vulkan::Headers
# comes from find_package(Vulkan); otherwise the vendored archive defines it.
FetchContent_Declare(VulkanHeaders
	URL          "https://${VULKAN_HEADERS_FILE}"
	DOWNLOAD_DIR "${CMAKE_SOURCE_DIR}/.cache/fetch/${VULKAN_HEADERS_FILE}"
	FIND_PACKAGE_ARGS NAMES Vulkan
)
FetchContent_MakeAvailable(VulkanHeaders)

# -------------------------------------------------------------------------
# The runtime stack (loader + MoltenVK ICD) needed by a runnable target lives
# in cmake/addVulkanRuntime.cmake — it provides Vulkan::Loader and the
# acm_stage_vulkan_runtime() helper. The static library needs none of it;
# only an executable that creates a VkInstance does (see testbed/).
# -------------------------------------------------------------------------
