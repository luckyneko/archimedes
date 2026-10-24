/*
 *  Created by LuckyNeko on 24/10/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

// A platform translation unit: one of the few allowed to include <windows.h>.
#ifdef _WIN32
#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#	endif
#	ifndef NOMINMAX
#		define NOMINMAX
#	endif
#	include <windows.h>
#endif

#include "archimedes/Platform.h"

#if defined(__APPLE__)
#	include <mach-o/dyld.h>
#endif

#include <cstdint>
#include <cstdlib>
#include <system_error>

namespace acm
{
	std::filesystem::path executableDirectory()
	{
		std::filesystem::path executable;
#if defined(_WIN32)
		std::wstring buffer(MAX_PATH, L'\0');
		for (;;)
		{
			const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), DWORD(buffer.size()));
			if (length == 0)
				return {};
			if (length < buffer.size())
			{
				buffer.resize(length);
				break;
			}
			buffer.resize(buffer.size() * 2); // truncated: grow and retry
		}
		executable = buffer;
#elif defined(__APPLE__)
		uint32_t size = 0;
		_NSGetExecutablePath(nullptr, &size); // first call reports the required size
		std::string buffer(size, '\0');
		if (_NSGetExecutablePath(buffer.data(), &size) != 0)
			return {};
		executable = buffer.c_str();
#elif defined(__linux__)
		executable = "/proc/self/exe";
#else
		return {};
#endif
		std::error_code ec;
		executable = std::filesystem::canonical(executable, ec);
		if (ec)
			return {};
		return executable.parent_path();
	}

	void setEnvDefault(const char* name, const std::string& value)
	{
		if (std::getenv(name))
			return;
#ifdef _WIN32
		_putenv_s(name, value.c_str());
#else
		setenv(name, value.c_str(), 0); // 0: do not overwrite
#endif
	}
} // namespace acm
