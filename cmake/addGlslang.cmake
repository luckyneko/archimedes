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
	# No system compiler found: fetch glslang and build its standalone. The
	# download path mirrors the source URL under .cache/fetch/ so similarly
	# named archives across deps can't collide; sources extract into build/_deps.
	include(FetchContent)
	# Share the single Vulkan SDK pin (acmVulkan) so glslang can't skew from the
	# headers/loader; falls back to a literal if acmVulkan wasn't included.
	if(NOT DEFINED ARCHIMEDES_VULKAN_SDK)
		set(ARCHIMEDES_VULKAN_SDK "1.4.341.0")
	endif()
	set(GLSLANG_VER "${ARCHIMEDES_VULKAN_SDK}" CACHE STRING "Vendored glslang SDK version")
	set(GLSLANG_FILE "github.com/KhronosGroup/glslang/archive/refs/tags/vulkan-sdk-${GLSLANG_VER}.tar.gz")

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

	FetchContent_Declare(glslang
		URL          "https://${GLSLANG_FILE}"
		DOWNLOAD_DIR "${CMAKE_SOURCE_DIR}/.cache/fetch/${GLSLANG_FILE}"
	)
	FetchContent_MakeAvailable(glslang)

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
