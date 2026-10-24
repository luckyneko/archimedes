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
#else
#	include <dlfcn.h>
#endif

#include "archimedes/PlatformLibrary.h"

namespace acm
{
	PlatformLibrary::PlatformLibrary(const std::filesystem::path& path)
	{
#if defined(_WIN32)
		m_handle = LoadLibraryW(path.c_str());
#else
		m_handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
	}

	void PlatformLibrary::reset()
	{
		if (!m_handle)
			return;
#if defined(_WIN32)
		FreeLibrary(static_cast<HMODULE>(m_handle));
#else
		dlclose(m_handle);
#endif
		m_handle = nullptr;
	}

	void* PlatformLibrary::symbol(const char* name) const
	{
		if (!m_handle)
			return nullptr;
#if defined(_WIN32)
		return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(m_handle), name));
#else
		return dlsym(m_handle, name);
#endif
	}
} // namespace acm
