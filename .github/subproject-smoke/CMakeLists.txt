cmake_minimum_required(VERSION 3.24)

project(archimedes-subproject-smoke LANGUAGES CXX)

if(NOT ARCHIMEDES_SOURCE_DIR)
	message(FATAL_ERROR "Set ARCHIMEDES_SOURCE_DIR to the Archimedes source tree")
endif()

add_subdirectory("${ARCHIMEDES_SOURCE_DIR}" archimedes)

add_executable(parent-smoke main.cpp)
target_link_libraries(parent-smoke PRIVATE archimedes)
