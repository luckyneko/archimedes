cmake_minimum_required(VERSION 3.20)

project(archimedes LANGUAGES CXX VERSION 0.1.0)

# Default build release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose Release or Debug" FORCE)
endif()

# Disable building in-source
if(CMAKE_BINARY_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(FATAL_ERROR "Building in-source is not supported! Create a build dir and remove ${CMAKE_SOURCE_DIR}/CMakeCache.txt")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Vendored dependencies (download into thirdparty/ on demand; git-ignored).
include(addspdlog)
include(addVulkan)

# glslc is only needed once we have shaders to compile; absent today.
find_program(GLSLC glslc)

function(vk_target_shaders TARGET)
    if(NOT ARGN)
        return()
    endif()

    foreach(SHADER_IN ${ARGN})
        get_filename_component(FILE_NAME ${SHADER_IN} NAME)
        set(SHADER_OUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/${FILE_NAME}.spv)
        add_custom_command(
            OUTPUT ${SHADER_OUT}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders/"
            COMMAND ${GLSLC} -Werror ${CMAKE_SOURCE_DIR}/${SHADER_IN} -o ${SHADER_OUT}
            DEPENDS ${CMAKE_SOURCE_DIR}/${SHADER_IN}
            COMMENT "Compiling shader ${SHADER_OUT}"
        )
        list(APPEND COMPILED_SHADERS ${SHADER_OUT})
    endforeach()

    add_custom_target(${TARGET}_shaders DEPENDS ${COMPILED_SHADERS})
    add_dependencies(${TARGET} ${TARGET}_shaders)
endfunction()

set(SOURCE_FILES
    src/acmDevice.cpp
    src/acmImage.cpp
    src/acmInstance.cpp
    src/acmRenderTarget.cpp
    src/acmSurface.cpp
    src/acmSwapChain.cpp
    src/acmUtils.cpp
)

# A static library only needs the Vulkan *headers* to compile; the loader is a
# link-time concern of an eventual executable. See cmake/addVulkan.cmake.
set(LIBRARIES
    spdlog::spdlog
    Vulkan::Headers
)

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
target_link_libraries(${PROJECT_NAME} PUBLIC ${LIBRARIES})
target_include_directories(${PROJECT_NAME} PUBLIC include)
target_compile_definitions(${PROJECT_NAME} PRIVATE ${DEFINES})
