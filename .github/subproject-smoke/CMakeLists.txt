cmake_minimum_required(VERSION 3.24)

project(archimedes-subproject-smoke LANGUAGES CXX)

if(NOT ARCHIMEDES_SOURCE_DIR)
	message(FATAL_ERROR "Set ARCHIMEDES_SOURCE_DIR to the Archimedes source tree")
endif()

add_subdirectory("${ARCHIMEDES_SOURCE_DIR}" archimedes)

# The library alone: a consumer that only compiles against archimedes.
add_executable(parent-smoke main.cpp)
target_link_libraries(parent-smoke PRIVATE archimedes)

# The runtime a consumer provisions itself. These calls run in this project's scope, so
# whatever archimedes' functions need (the CPU driver's pin among it) must reach them
# from here, not from archimedes' own directory scope.
acm_require_vulkan_runtime()
if(NOT ACM_SWIFTSHADER_ICD OR NOT ACM_SWIFTSHADER_LIB OR NOT ACM_SWIFTSHADER_LICENSES)
	message(FATAL_ERROR "the CPU driver (or its licence texts) did not reach the parent project")
endif()

add_executable(parent-runtime-smoke runtime.cpp)
target_link_libraries(parent-runtime-smoke PRIVATE archimedes Vulkan::Loader)
acm_stage_vulkan_runtime(parent-runtime-smoke)
