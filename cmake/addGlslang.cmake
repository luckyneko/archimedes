# Provides an offline GLSL -> SPIR-V compiler and the vk_target_shaders() helper.
#
# Prefers a system compiler (glslc from shaderc, or glslangValidator/glslang),
# otherwise vendors glslang from source and builds its standalone compiler — no
# system install required. Mirrors the other add*.cmake vendoring modules.
#
# Sets, for vk_target_shaders() (recomputed every configure — do NOT gate the
# vendored target's creation behind a cache var, or a reconfigure skips it while
# the cached compiler name dangles):
#   ACM_GLSL_COMPILER       command/target that compiles one shader
#   ACM_GLSL_COMPILER_FLAG  per-compiler flag (glslc needs none; glslang needs -V)

find_program(_acm_glslc NAMES glslc)
find_program(_acm_glslang NAMES glslangValidator glslang)

if(_acm_glslc)
	message(STATUS "Using system glslc: ${_acm_glslc}")
	set(ACM_GLSL_COMPILER "${_acm_glslc}")
	set(ACM_GLSL_COMPILER_FLAG "")
elseif(_acm_glslang)
	message(STATUS "Using system glslang: ${_acm_glslang}")
	set(ACM_GLSL_COMPILER "${_acm_glslang}")
	set(ACM_GLSL_COMPILER_FLAG "-V")
else()
	# --- Vendor glslang (standalone compiler) ----------------------------
	set(GLSLANG_VER "1.4.341.0" CACHE STRING "Vendored glslang SDK version")
	set(_gl_tag "vulkan-sdk-${GLSLANG_VER}")
	set(_gl_dir "${CMAKE_SOURCE_DIR}/thirdparty/glslang-${_gl_tag}")

	if(NOT EXISTS "${CMAKE_SOURCE_DIR}/thirdparty/glslang-${GLSLANG_VER}.tar.gz")
		message(STATUS "Downloading glslang (${_gl_tag})")
		file(DOWNLOAD
			"https://github.com/KhronosGroup/glslang/archive/refs/tags/${_gl_tag}.tar.gz"
			"${CMAKE_SOURCE_DIR}/thirdparty/glslang-${GLSLANG_VER}.tar.gz"
			STATUS _gl_dl_status
		)
		list(GET _gl_dl_status 0 _gl_dl_code)
		if(NOT _gl_dl_code EQUAL 0)
			file(REMOVE "${CMAKE_SOURCE_DIR}/thirdparty/glslang-${GLSLANG_VER}.tar.gz")
			list(GET _gl_dl_status 1 _gl_dl_msg)
			message(FATAL_ERROR "Failed to download glslang ${_gl_tag}: ${_gl_dl_msg}")
		endif()
	endif()

	if(NOT EXISTS "${_gl_dir}")
		message(STATUS "Decompress glslang (${_gl_tag})")
		execute_process(COMMAND
			${CMAKE_COMMAND} -E tar xfz "${CMAKE_SOURCE_DIR}/thirdparty/glslang-${GLSLANG_VER}.tar.gz"
			WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/thirdparty"
		)
	endif()

	message(STATUS "Using thirdparty/glslang (${_gl_tag})")
	# Build only the standalone compiler. ENABLE_OPT requires SPIRV-Tools
	# (not in the source archive), so it must be off.
	set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
	set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
	set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
	set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
	set(ENABLE_GLSLANG_BINARIES ON CACHE BOOL "" FORCE)
	set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
	set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
	set(ENABLE_CTEST OFF CACHE BOOL "" FORCE)
	if(NOT TARGET glslang-standalone)
		add_subdirectory(
			"${_gl_dir}"
			"${CMAKE_BINARY_DIR}/thirdparty/glslang-${_gl_tag}"
		)
	endif()

	# An executable target name is resolved to its built path (and added as a
	# build dependency) when used in add_custom_command COMMAND.
	set(ACM_GLSL_COMPILER "glslang-standalone")
	set(ACM_GLSL_COMPILER_FLAG "-V")
endif()

# Compiles GLSL sources to SPIR-V (<name>.spv) under <bindir>/shaders/ and makes
# <TARGET> depend on them.
function(vk_target_shaders TARGET)
	if(NOT ARGN)
		return()
	endif()

	set(_spvs "")
	foreach(SHADER_IN ${ARGN})
		get_filename_component(FILE_NAME ${SHADER_IN} NAME)
		set(SHADER_OUT "${CMAKE_CURRENT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
		add_custom_command(
			OUTPUT "${SHADER_OUT}"
			COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
			COMMAND ${ACM_GLSL_COMPILER} ${ACM_GLSL_COMPILER_FLAG} "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_IN}" -o "${SHADER_OUT}"
			DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_IN}"
			COMMENT "Compiling shader ${FILE_NAME}"
			VERBATIM
		)
		list(APPEND _spvs "${SHADER_OUT}")
	endforeach()

	add_custom_target(${TARGET}_shaders DEPENDS ${_spvs})
	add_dependencies(${TARGET} ${TARGET}_shaders)
endfunction()
