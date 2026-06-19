# Provides the GLFW window/input library for the testbed executable. Exposes the
# `glfw` target; only the testbed links it — the archimedes library itself has no
# windowing dependency.
#
# Prefers a system package; otherwise fetches a pinned release via FetchContent.
# The download path mirrors the source URL under .cache/fetch/ so similarly
# named archives across deps can't collide; sources extract into build/_deps.

include(FetchContent)

set(GLFW_VER "3.4" CACHE STRING "Vendored GLFW version")
set(GLFW_FILE "github.com/glfw/glfw/archive/refs/tags/${GLFW_VER}.tar.gz")

# Window/Vulkan-surface only — skip everything else GLFW can build.
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glfw3
	URL          "https://${GLFW_FILE}"
	DOWNLOAD_DIR "${CMAKE_SOURCE_DIR}/.cache/fetch/${GLFW_FILE}"
	FIND_PACKAGE_ARGS NAMES glfw3
)
FetchContent_MakeAvailable(glfw3)
