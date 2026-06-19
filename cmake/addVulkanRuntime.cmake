# Vendors the Vulkan *runtime* stack needed by a runnable target (the testbed),
# with no system install:
#
#   * Vulkan-Loader  — built from source against the vendored Vulkan::Headers,
#                      providing the Vulkan::Loader target to link against.
#   * MoltenVK       — downloaded prebuilt (the ICD that maps Vulkan -> Metal),
#                      staged next to the binary with its ICD manifest.
#
# Both are fetched via FetchContent: archives cached under .cache/fetch/ (path
# mirrors the source URL so similarly named tags don't collide), sources
# extracted into build/_deps.
#
# Include this only from a target that actually creates a VkInstance — the
# archimedes static library needs nothing here (headers suffice to compile).
# Validation layers are intentionally NOT vendored (macOS ships them only via
# the LunarG SDK); acm_stage_vulkan_runtime() enables them opportunistically
# when $VULKAN_SDK is present.

if(NOT TARGET Vulkan::Headers)
	message(FATAL_ERROR "addVulkanRuntime requires Vulkan::Headers — include(addVulkan) first.")
endif()

include(FetchContent)

# --- Vulkan-Loader (from source) -----------------------------------------
# Pinned to the same SDK series as the headers. Codegen (LOADER_CODEGEN) is
# off by default, so no Python is needed — releases ship generated sources.
set(VULKAN_LOADER_VER "1.4.341.0" CACHE STRING "Vendored Vulkan-Loader SDK version")
set(VULKAN_LOADER_FILE "github.com/KhronosGroup/Vulkan-Loader/archive/refs/tags/vulkan-sdk-${VULKAN_LOADER_VER}.tar.gz")

set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_WERROR OFF CACHE BOOL "" FORCE)
set(UPDATE_DEPS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(VulkanLoader
	URL          "https://${VULKAN_LOADER_FILE}"
	DOWNLOAD_DIR "${CMAKE_SOURCE_DIR}/.cache/fetch/${VULKAN_LOADER_FILE}"
)
FetchContent_MakeAvailable(VulkanLoader)

# On Apple the loader also builds vulkan.framework (a second full compile of
# the loader). We only link the plain `vulkan` dylib, so drop it from the
# default build to halve loader build time.
if(TARGET vulkan-framework)
	set_target_properties(vulkan-framework PROPERTIES EXCLUDE_FROM_ALL ON)
endif()

# --- MoltenVK (prebuilt ICD) ---------------------------------------------
if(APPLE)
	set(MOLTENVK_VER "1.4.1" CACHE STRING "Vendored MoltenVK release version")
	set(MOLTENVK_FILE "github.com/KhronosGroup/MoltenVK/releases/download/v${MOLTENVK_VER}/MoltenVK-macos.tar")

	# A prebuilt binary payload, not a CMake project — MakeAvailable just
	# populates it (no add_subdirectory), and we reference its files by path.
	FetchContent_Declare(MoltenVK
		URL          "https://${MOLTENVK_FILE}"
		DOWNLOAD_DIR "${CMAKE_SOURCE_DIR}/.cache/fetch/${MOLTENVK_FILE}"
	)
	FetchContent_MakeAvailable(MoltenVK)

	# The archive's single top-level MoltenVK/ dir is stripped on extraction
	# (ExternalProject behaviour), so the dylib + ICD sit one level shallower
	# than the raw tar layout.
	set(_mvk_payload "${moltenvk_SOURCE_DIR}/MoltenVK/dynamic/dylib/macOS")
	set(ACM_MOLTENVK_DYLIB "${_mvk_payload}/libMoltenVK.dylib" CACHE FILEPATH "Vendored libMoltenVK.dylib" FORCE)
	set(ACM_MOLTENVK_ICD   "${_mvk_payload}/MoltenVK_icd.json" CACHE FILEPATH "Vendored MoltenVK ICD manifest" FORCE)
	message(STATUS "Using vendored MoltenVK (${MOLTENVK_VER})")
endif()

# Stages the vendored runtime next to <target> and emits a launcher that points
# the loader at the staged MoltenVK ICD. Call after defining the target.
function(acm_stage_vulkan_runtime target)
	if(NOT APPLE)
		message(WARNING "acm_stage_vulkan_runtime: only the macOS/MoltenVK path is implemented.")
		return()
	endif()

	# The ICD json references ./libMoltenVK.dylib, so the manifest and the dylib
	# must sit in the same directory. The linked loader (libvulkan) is resolved
	# from its build dir via the rpath CMake adds automatically.
	add_custom_command(TARGET ${target} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/vulkan"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ACM_MOLTENVK_DYLIB}" "$<TARGET_FILE_DIR:${target}>/vulkan/"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ACM_MOLTENVK_ICD}"   "$<TARGET_FILE_DIR:${target}>/vulkan/"
		COMMENT "Staging MoltenVK ICD for ${target}"
		VERBATIM
	)

	set(_layer_env "")
	if(DEFINED ENV{VULKAN_SDK})
		set(_layer_env "export VK_LAYER_PATH=\"$ENV{VULKAN_SDK}/share/vulkan/explicit_layer.d\"\n")
	endif()
	file(GENERATE
		OUTPUT "${CMAKE_BINARY_DIR}/run_${target}.sh"
		CONTENT "#!/bin/sh
# Auto-generated: runs ${target} against the vendored MoltenVK ICD.
export VK_ICD_FILENAMES=\"$<TARGET_FILE_DIR:${target}>/vulkan/MoltenVK_icd.json\"
${_layer_env}exec \"$<TARGET_FILE:${target}>\" \"$@\"
"
		FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endfunction()
