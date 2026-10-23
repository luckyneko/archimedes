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

# The CPU driver (SwiftShader) staged beside runnable targets, so a CPU device is always
# available (docs/adr/0001). Archives come from a pinned luckyneko/swiftshader-prebuilt
# release, pinned in _acm_require_cpu_driver() below. Point ARCHIMEDES_SWIFTSHADER_DIR at
# a directory holding vk_swiftshader_icd.json and the library it names to stage a local
# build instead.
option(ARCHIMEDES_SOFTWARE_VULKAN "Stage the SwiftShader CPU driver beside runnable targets" ON)
set(ARCHIMEDES_SWIFTSHADER_DIR "" CACHE PATH "Local SwiftShader build to stage instead of the prebuilt (holds vk_swiftshader_icd.json)")
unset(ARCHIMEDES_SWIFTSHADER_VERSION CACHE) # once a cache entry; the pin is not cached

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
		# MoltenVK is Apache-2.0 too, so its licence ships with it like the CPU driver's.
		set(_mvk_licenses "")
		if(EXISTS "${moltenvk_SOURCE_DIR}/LICENSE")
			set(_mvk_licenses "${moltenvk_SOURCE_DIR}/LICENSE")
		endif()
		set(ACM_MOLTENVK_LICENSES "${_mvk_licenses}" CACHE STRING "MoltenVK's licence text, to ship with the driver" FORCE)
		message(STATUS "archimedes: vendored MoltenVK ${ARCHIMEDES_MOLTENVK_VERSION}")
	endif()

	_acm_require_cpu_driver()
endfunction()

# Provides the CPU driver (setting ACM_SWIFTSHADER_LIB / ACM_SWIFTSHADER_ICD), or leaves
# both unset when ARCHIMEDES_SOFTWARE_VULKAN is OFF or this host has no published
# archive. A missing archive warns rather than failing, so a default configure never breaks.
function(_acm_require_cpu_driver)
	# The pin: one release tag plus each platform's SHA256 (empty: no published archive).
	# It lives in this body, not at module scope, because a function sees its *caller's*
	# variables — a parent project calling acm_require_vulkan_runtime() never sees
	# archimedes' directory scope, while a function body is global once defined. Plain
	# variables, so a build folder's stale cache can never hold an older tag.
	set(ARCHIMEDES_SWIFTSHADER_VERSION "2026.10.14-cddb5c91")
	set(_sha256_linux-x64   "4eeeca5c8bf2f139090683544a6b3c8d841b67c68937bbe5b707c63ffef09745")
	set(_sha256_macos-arm64 "c0f211feb0b12abe6c66f30df43b88eecea05a30a766c8be89464a7f2ef2aeda")
	set(_sha256_windows-x64 "3153171c00faaa08e7df37d15c5a1caea77905855215639d3023392deb041ae9")

	unset(ACM_SWIFTSHADER_LIB CACHE)
	unset(ACM_SWIFTSHADER_ICD CACHE)
	unset(ACM_SWIFTSHADER_LICENSES CACHE)
	if(NOT ARCHIMEDES_SOFTWARE_VULKAN)
		return()
	endif()

	if(ARCHIMEDES_SWIFTSHADER_DIR)
		set(_payload "${ARCHIMEDES_SWIFTSHADER_DIR}")
		set(_origin "local build ${ARCHIMEDES_SWIFTSHADER_DIR}")
	else()
		string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _cpu)
		set(_platform "")
		if(APPLE AND _cpu MATCHES "^(arm64|aarch64)$")
			set(_platform "macos-arm64")
		elseif(WIN32 AND _cpu MATCHES "^(amd64|x86_64)$")
			set(_platform "windows-x64")
		elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND _cpu MATCHES "^(amd64|x86_64)$")
			set(_platform "linux-x64")
		endif()
		set(_sha256 "")
		if(_platform)
			set(_sha256 "${_sha256_${_platform}}")
		endif()
		if(NOT ARCHIMEDES_SWIFTSHADER_VERSION OR NOT _sha256)
			message(WARNING "archimedes: no SwiftShader prebuilt for ${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}; "
				"no CPU driver is staged. Set ARCHIMEDES_SWIFTSHADER_DIR to a local build, or "
				"ARCHIMEDES_SOFTWARE_VULKAN=OFF to silence this.")
			return()
		endif()

		set(_file "github.com/luckyneko/swiftshader-prebuilt/releases/download/${ARCHIMEDES_SWIFTSHADER_VERSION}/swiftshader-${ARCHIMEDES_SWIFTSHADER_VERSION}-${_platform}.tar.gz")
		# A prebuilt payload, not a CMake project — MakeAvailable just populates it.
		FetchContent_Declare(SwiftShader
			URL          "https://${_file}"
			URL_HASH     "SHA256=${_sha256}"
			DOWNLOAD_DIR "${CMAKE_SOURCE_DIR}/.cache/fetch/${_file}"
		)
		FetchContent_MakeAvailable(SwiftShader)
		set(_payload "${swiftshader_SOURCE_DIR}")
		set(_origin "SwiftShader ${ARCHIMEDES_SWIFTSHADER_VERSION} (${_platform})")
	endif()

	# The ICD json names its library relative to itself, so both are staged side by side.
	set(_icd "${_payload}/vk_swiftshader_icd.json")
	file(GLOB _lib "${_payload}/*vk_swiftshader${CMAKE_SHARED_LIBRARY_SUFFIX}")
	if(NOT EXISTS "${_icd}" OR NOT _lib)
		message(WARNING "archimedes: ${_origin} lacks vk_swiftshader_icd.json or its library; no CPU driver is staged.")
		return()
	endif()
	list(GET _lib 0 _lib)

	# Apache-2.0 redistribution ships the licence with the binary: SwiftShader's own and its
	# third-party notices (LLVM, marl, SPIRV-Tools, ...). The prebuilt carries both; a local
	# build folder usually has neither.
	set(_licenses "")
	foreach(_name IN ITEMS LICENSE.txt THIRD_PARTY_LICENSES.txt)
		if(EXISTS "${_payload}/${_name}")
			list(APPEND _licenses "${_payload}/${_name}")
		endif()
	endforeach()
	if(NOT _licenses)
		message(STATUS "archimedes: ${_origin} carries no licence texts; ACM_SWIFTSHADER_LICENSES is empty")
	endif()

	set(ACM_SWIFTSHADER_LIB "${_lib}" CACHE FILEPATH "Staged SwiftShader CPU driver library" FORCE)
	set(ACM_SWIFTSHADER_ICD "${_icd}" CACHE FILEPATH "Staged SwiftShader CPU driver ICD manifest" FORCE)
	set(ACM_SWIFTSHADER_LICENSES "${_licenses}" CACHE STRING "SwiftShader's licence texts, to ship with the CPU driver" FORCE)
	message(STATUS "archimedes: CPU driver from ${_origin}")
endfunction()

# The ICD manifests of every staged driver, as absolute paths beside their libraries.
function(_acm_staged_manifests out)
	set(_manifests "")
	if(ACM_MOLTENVK_ICD)
		list(APPEND _manifests "${ACM_MOLTENVK_ICD}")
	endif()
	if(ACM_SWIFTSHADER_ICD)
		list(APPEND _manifests "${ACM_SWIFTSHADER_ICD}")
	endif()
	set(${out} "${_manifests}" PARENT_SCOPE)
endfunction()

# The loader environment exposing <manifests> as one NAME=VALUE entry (empty for no
# manifests). macOS makes them the whole driver list, as MoltenVK's always was;
# elsewhere they are added to the system's drivers. Matches
# acm::useStagedVulkanRuntime() for launches that bypass it (ctest, launchers).
function(_acm_driver_environment out)
	set(_env "")
	if(ARGN)
		if(WIN32)
			list(JOIN ARGN "\;" _joined)
		else()
			list(JOIN ARGN ":" _joined)
		endif()
		if(APPLE)
			set(_env "VK_ICD_FILENAMES=${_joined}")
		else()
			set(_env "VK_ADD_DRIVER_FILES=${_joined}")
		endif()
	endif()
	set(${out} "${_env}" PARENT_SCOPE)
endfunction()

# The loader environment (one NAME=VALUE entry, or empty) that exposes every staged
# driver from where it was provisioned — for ctest and other launches that run a
# binary without its staged copies being known at configure time.
function(acm_vulkan_driver_environment out)
	_acm_staged_manifests(_manifests)
	_acm_driver_environment(_env ${_manifests})
	set(${out} "${_env}" PARENT_SCOPE)
endfunction()

# Stages every staged driver (MoltenVK on macOS, the CPU driver where available) in
# <target>'s <bindir>/vulkan/ and, off Windows, emits build/run_<target>.sh that points
# the loader at them. A no-op when nothing is staged (e.g. Linux with the CPU driver
# OFF). Call after the target is defined and acm_require_vulkan_runtime() has run.
function(acm_stage_vulkan_runtime target)
	set(_files "")
	if(ACM_MOLTENVK_ICD)
		list(APPEND _files "${ACM_MOLTENVK_DYLIB}" "${ACM_MOLTENVK_ICD}")
	endif()
	if(ACM_SWIFTSHADER_ICD)
		list(APPEND _files "${ACM_SWIFTSHADER_LIB}" "${ACM_SWIFTSHADER_ICD}")
	endif()
	if(NOT _files)
		return()
	endif()

	# Each staged driver's licence texts ship beside it, prefixed with the driver's name so
	# they are not mistaken for the application's own (vk_swiftshader.LICENSE.txt,
	# MoltenVK.LICENSE).
	set(_license_commands "")
	if(ACM_SWIFTSHADER_ICD)
		foreach(_license IN LISTS ACM_SWIFTSHADER_LICENSES)
			get_filename_component(_name "${_license}" NAME)
			list(APPEND _license_commands
				COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_license}" "$<TARGET_FILE_DIR:${target}>/vulkan/vk_swiftshader.${_name}")
		endforeach()
	endif()
	if(ACM_MOLTENVK_ICD)
		foreach(_license IN LISTS ACM_MOLTENVK_LICENSES)
			get_filename_component(_name "${_license}" NAME)
			list(APPEND _license_commands
				COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_license}" "$<TARGET_FILE_DIR:${target}>/vulkan/MoltenVK.${_name}")
		endforeach()
	endif()

	# Each ICD json references its library as ./<lib>, so the manifests and libraries
	# must sit in the same directory. The linked loader (libvulkan) is resolved from its
	# build dir via the rpath CMake adds automatically.
	add_custom_command(TARGET ${target} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/vulkan"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_files} "$<TARGET_FILE_DIR:${target}>/vulkan/"
		${_license_commands}
		COMMENT "Staging Vulkan drivers for ${target}"
		VERBATIM
	)

	if(WIN32)
		return()
	endif()
	_acm_staged_manifests(_manifests)
	set(_staged "")
	foreach(_manifest IN LISTS _manifests)
		get_filename_component(_name "${_manifest}" NAME)
		list(APPEND _staged "$<TARGET_FILE_DIR:${target}>/vulkan/${_name}")
	endforeach()
	_acm_driver_environment(_env ${_staged})
	set(_layer_env "")
	if(DEFINED ENV{VULKAN_SDK})
		set(_layer_env "export VK_LAYER_PATH=\"$ENV{VULKAN_SDK}/share/vulkan/explicit_layer.d\"\n")
	endif()
	string(REGEX REPLACE "^([A-Z_]+)=(.*)$" "\\1=\"\\2\"" _export "${_env}")
	file(GENERATE
		OUTPUT "${CMAKE_BINARY_DIR}/run_${target}.sh"
		CONTENT "#!/bin/sh
# Auto-generated: runs ${target} against its staged Vulkan drivers.
export ${_export}
${_layer_env}exec \"$<TARGET_FILE:${target}>\" \"$@\"
"
		FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endfunction()
