# Provides the GLFW window/input library for the testbed executable.
#
# Mirrors cmake/addspdlog.cmake / addVulkan.cmake: prefer a system package,
# otherwise download and vendor a pinned release into thirdparty/ (git-ignored)
# on demand. Exposes the `glfw` target. Only the testbed links this — the
# archimedes library itself has no windowing dependency.

find_package(glfw3 CONFIG QUIET)

if (TARGET glfw)
	message(STATUS "Using system GLFW (${glfw3_VERSION})")
else ()
	set(GLFW_VER "3.4" CACHE STRING "Vendored GLFW version")
	set(_glfw_dir "${CMAKE_SOURCE_DIR}/thirdparty/glfw-${GLFW_VER}")

	if(NOT EXISTS "${CMAKE_SOURCE_DIR}/thirdparty/glfw-${GLFW_VER}.tar.gz")
		message(STATUS "Downloading GLFW (${GLFW_VER})")
		file(DOWNLOAD
			"https://github.com/glfw/glfw/archive/refs/tags/${GLFW_VER}.tar.gz"
			"${CMAKE_SOURCE_DIR}/thirdparty/glfw-${GLFW_VER}.tar.gz"
			STATUS _glfw_dl_status
		)
		list(GET _glfw_dl_status 0 _glfw_dl_code)
		if(NOT _glfw_dl_code EQUAL 0)
			file(REMOVE "${CMAKE_SOURCE_DIR}/thirdparty/glfw-${GLFW_VER}.tar.gz")
			list(GET _glfw_dl_status 1 _glfw_dl_msg)
			message(FATAL_ERROR "Failed to download GLFW ${GLFW_VER}: ${_glfw_dl_msg}")
		endif()
	endif()

	if(NOT EXISTS "${_glfw_dir}")
		message(STATUS "Decompress GLFW (${GLFW_VER})")
		execute_process(COMMAND
			${CMAKE_COMMAND} -E tar xfz "${CMAKE_SOURCE_DIR}/thirdparty/glfw-${GLFW_VER}.tar.gz"
			WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/thirdparty"
		)
	endif()

	message(STATUS "Using thirdparty/glfw (${GLFW_VER})")
	# Window/Vulkan-surface only — skip everything else GLFW can build.
	set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
	set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
	set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
	if(NOT TARGET glfw)
		add_subdirectory(
			"${_glfw_dir}"
			"${CMAKE_BINARY_DIR}/thirdparty/glfw-${GLFW_VER}"
		)
	endif()
endif ()
