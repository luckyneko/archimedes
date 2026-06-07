# Provides a Vulkan target for archimedes to build against.
#
# Mirrors cmake/addspdlog.cmake: prefer a system package, otherwise download
# and vendor a pinned release into thirdparty/ (git-ignored) on demand.
#
# "Vulkan" is several separate things. archimedes is a STATIC library, so it
# only needs the Vulkan *headers* to compile — the loader, the MoltenVK ICD,
# and the validation layers are link/run-time concerns of an eventual
# executable, not of this archive. We therefore vendor only Vulkan-Headers
# here and expose the include-only Vulkan::Headers target.
#
# When a real Vulkan SDK is present (VULKAN_SDK / find_package), we defer to it
# so this file is a no-op on a properly provisioned machine.

# --- Prefer a system / SDK install ---------------------------------------
# A full SDK gives us Vulkan::Headers (and Vulkan::Vulkan) for free.
find_package(Vulkan QUIET)

if (TARGET Vulkan::Headers)
	message(STATUS "Using system Vulkan (${Vulkan_VERSION})")
else ()
	# --- Vendor Vulkan-Headers -------------------------------------------
	# Pinned to a Vulkan SDK tag; override with -DVULKAN_HEADERS_VER=...
	set(VULKAN_HEADERS_VER "1.4.341.0" CACHE STRING "Vendored Vulkan-Headers SDK version")
	set(_vkh_tag "vulkan-sdk-${VULKAN_HEADERS_VER}")
	set(_vkh_dir "${CMAKE_SOURCE_DIR}/thirdparty/Vulkan-Headers-${_vkh_tag}")

	if(NOT EXISTS "${CMAKE_SOURCE_DIR}/thirdparty/Vulkan-Headers-${VULKAN_HEADERS_VER}.tar.gz")
		message(STATUS "Downloading Vulkan-Headers (${_vkh_tag})")
		file(DOWNLOAD
			"https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/${_vkh_tag}.tar.gz"
			"${CMAKE_SOURCE_DIR}/thirdparty/Vulkan-Headers-${VULKAN_HEADERS_VER}.tar.gz"
			STATUS _vkh_dl_status
		)
		list(GET _vkh_dl_status 0 _vkh_dl_code)
		if(NOT _vkh_dl_code EQUAL 0)
			file(REMOVE "${CMAKE_SOURCE_DIR}/thirdparty/Vulkan-Headers-${VULKAN_HEADERS_VER}.tar.gz")
			list(GET _vkh_dl_status 1 _vkh_dl_msg)
			message(FATAL_ERROR "Failed to download Vulkan-Headers ${_vkh_tag}: ${_vkh_dl_msg}")
		endif()
	endif()

	if(NOT EXISTS "${_vkh_dir}")
		message(STATUS "Decompress Vulkan-Headers (${_vkh_tag})")
		execute_process(COMMAND
			${CMAKE_COMMAND} -E tar xfz "${CMAKE_SOURCE_DIR}/thirdparty/Vulkan-Headers-${VULKAN_HEADERS_VER}.tar.gz"
			WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/thirdparty"
		)
	endif()

	message(STATUS "Using thirdparty/Vulkan-Headers (${_vkh_tag})")
	# Defines the include-only INTERFACE target Vulkan::Headers.
	if(NOT TARGET Vulkan::Headers)
		add_subdirectory(
			"${_vkh_dir}"
			"${CMAKE_BINARY_DIR}/thirdparty/Vulkan-Headers-${_vkh_tag}"
		)
	endif()
endif ()

# -------------------------------------------------------------------------
# The runtime stack (loader + MoltenVK ICD) needed by a runnable target lives
# in cmake/addVulkanRuntime.cmake — it provides Vulkan::Loader and the
# acm_stage_vulkan_runtime() helper. The static library needs none of it;
# only an executable that creates a VkInstance does (see testbed/).
# -------------------------------------------------------------------------
