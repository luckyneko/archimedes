# Sanitizer instrumentation for archimedes' own targets, driven by the ARCHIMEDES_SANITIZE
# cache option (root CMakeLists.txt): empty for none, or a comma- or semicolon-separated
# list of thread, address, undefined and leak. Only targets passed to acm_sanitize() are
# instrumented — the library, the tests and the benchmarks — never the vendored
# dependencies (loader, Catch2, GLFW, glslang) or the prebuilt drivers, which TSan and ASan
# tolerate uninstrumented (test/tsan.supp covers the CPU driver).
#
# The static library compiles instrumented and carries the sanitizer link flag as a usage
# requirement, so every executable linking it (the testbed, a parent project's binary)
# links the matching runtime without calling this itself.

include_guard(GLOBAL)

# The validated, lower-case sanitizer list in <out> (empty when ARCHIMEDES_SANITIZE is).
function(acm_sanitizers out)
	set(_list "")
	if(ARCHIMEDES_SANITIZE)
		string(TOLOWER "${ARCHIMEDES_SANITIZE}" _list)
		string(REPLACE "," ";" _list "${_list}")
		list(TRANSFORM _list STRIP)
		list(REMOVE_ITEM _list "")
		list(REMOVE_DUPLICATES _list)
		foreach(_name IN LISTS _list)
			if(NOT _name MATCHES "^(thread|address|undefined|leak)$")
				message(FATAL_ERROR "ARCHIMEDES_SANITIZE: unknown sanitizer '${_name}' (use thread, address, undefined, leak)")
			endif()
		endforeach()
		if("thread" IN_LIST _list AND ("address" IN_LIST _list OR "leak" IN_LIST _list))
			message(FATAL_ERROR "ARCHIMEDES_SANITIZE: thread cannot be combined with address or leak")
		endif()
		if(_list AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
			message(FATAL_ERROR "ARCHIMEDES_SANITIZE needs Clang or GCC (the compiler is ${CMAKE_CXX_COMPILER_ID})")
		endif()
	endif()
	set(${out} "${_list}" PARENT_SCOPE)
endfunction()

# Instruments <target> with ARCHIMEDES_SANITIZE; a no-op when it is empty.
function(acm_sanitize target)
	acm_sanitizers(_list)
	if(NOT _list)
		return()
	endif()
	list(JOIN _list "," _flag)
	set(_compile -fsanitize=${_flag} -fno-omit-frame-pointer)
	if("undefined" IN_LIST _list)
		# Undefined behaviour aborts the process, so a test that hits it fails.
		list(APPEND _compile -fno-sanitize-recover=undefined)
	endif()
	target_compile_options(${target} PRIVATE ${_compile})

	get_target_property(_type ${target} TYPE)
	if(_type STREQUAL "STATIC_LIBRARY")
		target_link_options(${target} INTERFACE -fsanitize=${_flag})
	else()
		target_link_options(${target} PRIVATE -fsanitize=${_flag})
	endif()
endfunction()
