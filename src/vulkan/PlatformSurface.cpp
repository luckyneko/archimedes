/*
 *  Created by LuckyNeko on 04/07/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

// Platform window/surface glue for headless rendering. This is the one translation
// unit allowed to pull in <windows.h>, so the Win32 surface fallback stays isolated
// from the backend-neutral surface code. The Vulkan platform define must precede the
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
} // namespace acm::vulkan
