cmake_minimum_required(VERSION 3.24) # FetchContent FIND_PACKAGE_ARGS

# Detect top-level vs. bundled-as-a-subproject before our own project() call.
if(NOT DEFINED PROJECT_NAME)
    set(NOT_SUBPROJECT ON)
endif()

# C is enabled for the vendored Vulkan-Loader / GLFW sources (testbed only).
project(archimedes LANGUAGES CXX C VERSION 0.1.0)

# Default build release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose Release or Debug" FORCE)
endif()

option(ARCHIMEDES_BUILD_TESTBED "Build the testbed executable" ${NOT_SUBPROJECT})
option(ARCHIMEDES_BUILD_TESTING "Build the unit/integration tests" ${NOT_SUBPROJECT})

# Disable building in-source
if(CMAKE_BINARY_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(FATAL_ERROR "Building in-source is not supported! Create a build dir and remove ${CMAKE_SOURCE_DIR}/CMakeCache.txt")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Vendored dependencies. Each add*.cmake prefers a system package, otherwise
# fetches a pinned release via FetchContent — archives cached under
# .cache/fetch/ (git-ignored), sources extracted into build/_deps.
# Shader compilation (addGlslang) + the Vulkan runtime live with the testbed.
include(addVulkan)

set(SOURCE_FILES
    src/acmBuffer.cpp
    src/acmCommandBuffer.cpp
    src/acmCommandPool.cpp
    src/acmComputePipeline.cpp
    src/acmDescriptorSet.cpp
    src/acmDescriptorSetLayout.cpp
    src/acmDevice.cpp
    src/acmInstance.cpp
    src/acmPipeline.cpp
    src/acmRenderTarget.cpp
    src/acmRenderer.cpp
    src/acmSampler.cpp
    src/acmShader.cpp
    src/acmSurface.cpp
    src/acmSwapChain.cpp
    src/acmTexture.cpp
    src/acmUniformRing.cpp
    src/acmVkConvert.cpp
    src/acmVkMemory.cpp
    src/acmVkOneShot.cpp
)

# A static library only needs the Vulkan *headers* to compile; the loader is a
# link-time concern of an eventual executable. See cmake/addVulkan.cmake.
set(LIBRARIES
    Vulkan::Headers
)

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
target_link_libraries(${PROJECT_NAME} PUBLIC ${LIBRARIES})
target_include_directories(${PROJECT_NAME} PUBLIC include)
target_compile_definitions(${PROJECT_NAME} PRIVATE ${DEFINES})

if(ARCHIMEDES_BUILD_TESTBED)
    add_subdirectory(testbed)
endif()

if(ARCHIMEDES_BUILD_TESTING)
    enable_testing() # must be at the project root for ctest to find tests
    include(addcatch2)
    add_subdirectory(test)
endif()
