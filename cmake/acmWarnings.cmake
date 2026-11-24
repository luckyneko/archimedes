# Compiler warnings for archimedes' own targets: the library, the tests, the benchmarks and
# the testbed. Never applied to the vendored dependencies (loader, Catch2, GLFW, glslang),
# whose warnings are not ours to fix.
#
# The options are PRIVATE, so they cover only the sources of the target passed in: a
# project consuming the library through add_subdirectory compiles its own code with its
# own flags. ARCHIMEDES_WARNINGS_AS_ERRORS (root CMakeLists.txt) turns every warning into
# an error; it defaults to ON for a top-level build and OFF when archimedes is a subproject.

include_guard(GLOBAL)

# Turns on the warning level for <target>: /W4 on MSVC (and clang-cl), -Wall -Wextra on
# GCC and Clang, plus /WX or -Werror when ARCHIMEDES_WARNINGS_AS_ERRORS is on.
function(acm_warnings target)
	if(MSVC)
		set(_options /W4)
		if(ARCHIMEDES_WARNINGS_AS_ERRORS)
			list(APPEND _options /WX)
		endif()
	elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
		set(_options -Wall -Wextra)
		if(ARCHIMEDES_WARNINGS_AS_ERRORS)
			list(APPEND _options -Werror)
		endif()
	else()
		return()
	endif()
	target_compile_options(${target} PRIVATE ${_options})
endfunction()
