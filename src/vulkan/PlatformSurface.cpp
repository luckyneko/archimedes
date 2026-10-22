/*
 *  Created by LuckyNeko on 04/07/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

// Platform glue: the window behind a headless surface, the running executable's location
// (for finding staged drivers), and loading a driver library directly. This is the one translation unit allowed to
// pull in <windows.h>, so the Win32 code stays isolated from the backend-neutral code. The Vulkan platform define must precede the
// first <vulkan/vulkan.h> include to expose vkCreateWin32SurfaceKHR, so windows.h and
// the define come first.
#ifdef _WIN32
#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#	endif
#	ifndef NOMINMAX
#		define NOMINMAX
#	endif
#	include <windows.h>
#	define VK_USE_PLATFORM_WIN32_KHR
#endif

// Pulls in <vulkan/vulkan.h> (with the Win32 platform surface, per the define above).
#include "archimedes/vulkan/PlatformSurface.h"

#if defined(__APPLE__)
#	include <mach-o/dyld.h>
#endif
#if !defined(_WIN32)
#	include <dlfcn.h>
#endif

#include <cstdint>
#include <string>
#include <system_error>

namespace acm::vulkan
{
	// -----------------------------------------------------------------------------
	// PlatformWindow
	// -----------------------------------------------------------------------------

	void PlatformWindow::reset()
	{
#ifdef _WIN32
		if (m_handle)
			DestroyWindow(static_cast<HWND>(m_handle));
#endif
		m_handle = nullptr;
	}

	// -----------------------------------------------------------------------------
	// Surface creation
	// -----------------------------------------------------------------------------

	HeadlessSurface createHeadlessSurface(VkInstance instance, [[maybe_unused]] acm::Extent2D extent)
	{
		HeadlessSurface result;

		// Prefer the true windowless VK_EXT_headless_surface path (MoltenVK, software
		// rasterizers). It needs no backing window, so result.window stays empty.
		if (auto fn = reinterpret_cast<PFN_vkCreateHeadlessSurfaceEXT>(
				vkGetInstanceProcAddr(instance, "vkCreateHeadlessSurfaceEXT")))
		{
			VkHeadlessSurfaceCreateInfoEXT info = {};
			info.sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT;
			if (fn(instance, &info, nullptr, &result.surface) == VK_SUCCESS)
				return result;
			result.surface = VK_NULL_HANDLE;
		}

#ifdef _WIN32
		// Fallback: an off-screen Win32 window. VK_KHR_win32_surface is universal on
		// desktop ICDs and the instance already enables it via its VK_*_surface match.
		// WS_POPUP (no border/title bar) so the client area equals the window size — a
		// bordered window could collapse to a zero-height client area and yield a
		// degenerate surface extent. The window is never shown. PlatformWindow owns it
		// from creation, so every early return below tears it down automatically.
		const int width = int(extent.width ? extent.width : 1);
		const int height = int(extent.height ? extent.height : 1);
		HWND hwnd = CreateWindowExW(0, L"STATIC", L"archimedes-headless", WS_POPUP,
									0, 0, width, height, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
		if (!hwnd)
			return result;
		PlatformWindow window(hwnd);

		auto fn = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(
			vkGetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR"));
		if (!fn)
			return result;

		VkWin32SurfaceCreateInfoKHR info = {};
		info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		info.hinstance = GetModuleHandleW(nullptr);
		info.hwnd = hwnd;
		if (fn(instance, &info, nullptr, &result.surface) != VK_SUCCESS)
		{
			result.surface = VK_NULL_HANDLE;
			return result;
		}
		result.window = std::move(window);
#endif

		return result;
	}

	// -----------------------------------------------------------------------------
	// Executable location
	// -----------------------------------------------------------------------------

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

	// -----------------------------------------------------------------------------
	// PlatformLibrary
	// -----------------------------------------------------------------------------

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
} // namespace acm::vulkan
