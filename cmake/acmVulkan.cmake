# archimedes' Vulkan provisioning, exposed as acm_-prefixed getter functions a
# parent project can reuse. Vulkan is the one external dependency archimedes itself
# needs (headers to compile; consumers building archimedes-based executables need a
# matching loader), so it is the only thing archimedes passes through.
#
# The getters are:
#   * idempotent  — each is a no-op if the target already exists, so a consumer
#                   can supply its own Vulkan (a real SDK, its own FetchContent)
#                   by defining Vulkan::Headers / Vulkan::Loader first;
#   * version-locked — a single ARCHIMEDES_VULKAN_SDK pin drives headers + loader
#                   (and the MoltenVK ICD pairs against it), so nothing can skew;
#   * reusable from a parent — being functions (not bare module code), their
#                   definitions persist after add_subdirectory(archimedes), so a
#                   consumer calls acm_require_vulkan_runtime() without touching
#                   archimedes' module path or include()-ing its internals.
#
# Only Vulkan::Headers / Vulkan::Loader (the canonical imported-target names) are
# unprefixed; everything archimedes-specific is acm_ / ARCHIMEDES_ prefixed, so
# this collides with nothing in the consumer.

include_guard(GLOBAL)
include(FetchContent)

# Single source of truth for the Khronos SDK series. Override with -D if needed.
set(ARCHIMEDES_VULKAN_SDK "1.4.341.0" CACHE STRING "Vulkan SDK series archimedes vendors (headers + loader)")
set(ARCHIMEDES_MOLTENVK_VERSION "1.4.1" CACHE STRING "MoltenVK release (macOS portability ICD)")

# Ensure the Vulkan::Headers target exists. Prefers a real SDK (find_package),
# else vendors Vulkan-Headers at the pinned SDK. A static library only needs the
# headers to compile — the loader is an executable's concern (see below).
function(acm_require_vulkan_headers)
	if(TARGET Vulkan::Headers)
		return()
	endif()

	set(_file "github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/vulkan-sdk-${ARCHIMEDES_VULKAN_SDK}.tar.gz")
	FetchContent_Declare(VulkanHeaders
		URL          "https://${_file}"
		DOWNLOAD_DIR "${CMAKE_SOURCE_DIR}/.cache/fetch/${_file}"
		FIND_PACKAGE_ARGS NAMES Vulkan
	)
	FetchContent_MakeAvailable(VulkanHeaders)
endfunction()

# Ensure the Vulkan::Loader target exists: build the loader from source against
# the headers (cross-platform), and on macOS additionally fetch the prebuilt
# MoltenVK ICD (setting ACM_MOLTENVK_DYLIB / ACM_MOLTENVK_ICD). A runnable target
# that creates a VkInstance links Vulkan::Loader to resolve archimedes' vk*
# symbols. No system SDK required — the built loader finds the platform's
# installed ICD at runtime (MoltenVK on macOS, the system driver elsewhere).
function(acm_require_vulkan_runtime)
	if(TARGET Vulkan::Loader)
		return()
	endif()
	acm_require_vulkan_headers()

	set(_file "github.com/KhronosGroup/Vulkan-Loader/archive/refs/tags/vulkan-sdk-${ARCHIMEDES_VULKAN_SDK}.tar.gz")
	set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(BUILD_WERROR OFF CACHE BOOL "" FORCE)
	set(UPDATE_DEPS OFF CACHE BOOL "" FORCE)
	FetchContent_Declare(VulkanLoader
		URL          "https://${_file}"
		DOWNLOAD_DIR "${CMAKE_SOURCE_DIR}/.cache/fetch/${_file}"
	)
	FetchContent_MakeAvailable(VulkanLoader)

	# On Apple the loader also builds vulkan-framework (a second full loader
	# compile); we only link the plain loader, so drop it from the default build.
	if(TARGET vulkan-framework)
		set_target_properties(vulkan-framework PROPERTIES EXCLUDE_FROM_ALL ON)
	endif()

	if(APPLE)
		set(_mvk "github.com/KhronosGroup/MoltenVK/releases/download/v${ARCHIMEDES_MOLTENVK_VERSION}/MoltenVK-macos.tar")
		# A prebuilt payload, not a CMake project — MakeAvailable just populates it.
		FetchContent_Declare(MoltenVK
			URL          "https://${_mvk}"
			DOWNLOAD_DIR "${CMAKE_SOURCE_DIR}/.cache/fetch/${_mvk}"
		)
		FetchContent_MakeAvailable(MoltenVK)

		set(_payload "${moltenvk_SOURCE_DIR}/MoltenVK/dynamic/dylib/macOS")
		set(ACM_MOLTENVK_DYLIB "${_payload}/libMoltenVK.dylib" CACHE FILEPATH "Vendored libMoltenVK.dylib" FORCE)
		set(ACM_MOLTENVK_ICD   "${_payload}/MoltenVK_icd.json" CACHE FILEPATH "Vendored MoltenVK ICD manifest" FORCE)
		message(STATUS "archimedes: vendored MoltenVK ${ARCHIMEDES_MOLTENVK_VERSION}")
	endif()
endfunction()

# Stages the vendored MoltenVK ICD next to <target> and emits a launcher that
# points the loader at it. macOS only — a silent no-op elsewhere (Linux/Windows
# use the system-installed ICD, so there is nothing to stage). Call after the
# target is defined and acm_require_vulkan_runtime() has run.
function(acm_stage_vulkan_runtime target)
	if(NOT APPLE)
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
