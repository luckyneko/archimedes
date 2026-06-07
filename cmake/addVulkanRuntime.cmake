# Vendors the Vulkan *runtime* stack needed by a runnable target (the testbed),
# with no system install:
#
#   * Vulkan-Loader  — built from source against the vendored Vulkan::Headers,
#                      providing the Vulkan::Loader target to link against.
#   * MoltenVK       — downloaded prebuilt (the ICD that maps Vulkan -> Metal),
#                      staged next to the binary with its ICD manifest.
#
# Include this only from a target that actually creates a VkInstance — the
# archimedes static library needs nothing here (headers suffice to compile).
# Validation layers are intentionally NOT vendored (macOS ships them only via
# the LunarG SDK); acm_stage_vulkan_runtime() enables them opportunistically
# when $VULKAN_SDK is present.

if(NOT TARGET Vulkan::Headers)
	message(FATAL_ERROR "addVulkanRuntime requires Vulkan::Headers — include(addVulkan) first.")
endif()

# --- Vulkan-Loader (from source) -----------------------------------------
# Pinned to the same SDK series as the headers. Codegen (LOADER_CODEGEN) is
# off by default, so no Python is needed — releases ship generated sources.
if(NOT TARGET Vulkan::Loader)
	set(VULKAN_LOADER_VER "1.4.341.0" CACHE STRING "Vendored Vulkan-Loader SDK version")
	set(_vkl_tag "vulkan-sdk-${VULKAN_LOADER_VER}")
	set(_vkl_dir "${CMAKE_SOURCE_DIR}/thirdparty/Vulkan-Loader-${_vkl_tag}")

	if(NOT EXISTS "${CMAKE_SOURCE_DIR}/thirdparty/Vulkan-Loader-${VULKAN_LOADER_VER}.tar.gz")
		message(STATUS "Downloading Vulkan-Loader (${_vkl_tag})")
		file(DOWNLOAD
			"https://github.com/KhronosGroup/Vulkan-Loader/archive/refs/tags/${_vkl_tag}.tar.gz"
			"${CMAKE_SOURCE_DIR}/thirdparty/Vulkan-Loader-${VULKAN_LOADER_VER}.tar.gz"
			STATUS _vkl_dl_status
		)
		list(GET _vkl_dl_status 0 _vkl_dl_code)
		if(NOT _vkl_dl_code EQUAL 0)
			file(REMOVE "${CMAKE_SOURCE_DIR}/thirdparty/Vulkan-Loader-${VULKAN_LOADER_VER}.tar.gz")
			list(GET _vkl_dl_status 1 _vkl_dl_msg)
			message(FATAL_ERROR "Failed to download Vulkan-Loader ${_vkl_tag}: ${_vkl_dl_msg}")
		endif()
	endif()

	if(NOT EXISTS "${_vkl_dir}")
		message(STATUS "Decompress Vulkan-Loader (${_vkl_tag})")
		execute_process(COMMAND
			${CMAKE_COMMAND} -E tar xfz "${CMAKE_SOURCE_DIR}/thirdparty/Vulkan-Loader-${VULKAN_LOADER_VER}.tar.gz"
			WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/thirdparty"
		)
	endif()

	message(STATUS "Using thirdparty/Vulkan-Loader (${_vkl_tag})")
	set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(BUILD_WERROR OFF CACHE BOOL "" FORCE)
	set(UPDATE_DEPS OFF CACHE BOOL "" FORCE)
	add_subdirectory(
		"${_vkl_dir}"
		"${CMAKE_BINARY_DIR}/thirdparty/Vulkan-Loader-${_vkl_tag}"
	)

	# On Apple the loader also builds vulkan.framework (a second full compile of
	# the loader). We only link the plain `vulkan` dylib, so drop it from the
	# default build to halve loader build time.
	if(TARGET vulkan-framework)
		set_target_properties(vulkan-framework PROPERTIES EXCLUDE_FROM_ALL ON)
	endif()
endif()

# --- MoltenVK (prebuilt ICD) ---------------------------------------------
if(APPLE)
	set(MOLTENVK_VER "1.4.1" CACHE STRING "Vendored MoltenVK release version")
	set(_mvk_tar "${CMAKE_SOURCE_DIR}/thirdparty/MoltenVK-macos-${MOLTENVK_VER}.tar")
	set(_mvk_dir "${CMAKE_SOURCE_DIR}/thirdparty/MoltenVK-${MOLTENVK_VER}")
	set(_mvk_payload "${_mvk_dir}/MoltenVK/MoltenVK/dynamic/dylib/macOS")

	if(NOT EXISTS "${_mvk_tar}")
		message(STATUS "Downloading MoltenVK (${MOLTENVK_VER})")
		file(DOWNLOAD
			"https://github.com/KhronosGroup/MoltenVK/releases/download/v${MOLTENVK_VER}/MoltenVK-macos.tar"
			"${_mvk_tar}"
			STATUS _mvk_dl_status
			SHOW_PROGRESS
		)
		list(GET _mvk_dl_status 0 _mvk_dl_code)
		if(NOT _mvk_dl_code EQUAL 0)
			file(REMOVE "${_mvk_tar}")
			list(GET _mvk_dl_status 1 _mvk_dl_msg)
			message(FATAL_ERROR "Failed to download MoltenVK ${MOLTENVK_VER}: ${_mvk_dl_msg}")
		endif()
	endif()

	if(NOT EXISTS "${_mvk_payload}/libMoltenVK.dylib")
		message(STATUS "Decompress MoltenVK (${MOLTENVK_VER})")
		file(MAKE_DIRECTORY "${_mvk_dir}")
		execute_process(COMMAND
			${CMAKE_COMMAND} -E tar xf "${_mvk_tar}"
			WORKING_DIRECTORY "${_mvk_dir}"
		)
	endif()

	set(ACM_MOLTENVK_DYLIB "${_mvk_payload}/libMoltenVK.dylib" CACHE FILEPATH "Vendored libMoltenVK.dylib" FORCE)
	set(ACM_MOLTENVK_ICD   "${_mvk_payload}/MoltenVK_icd.json" CACHE FILEPATH "Vendored MoltenVK ICD manifest" FORCE)
	message(STATUS "Using thirdparty/MoltenVK (${MOLTENVK_VER})")
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
