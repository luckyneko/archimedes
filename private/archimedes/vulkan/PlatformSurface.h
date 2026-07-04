/*
 *  Created by LuckyNeko on 04/07/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmTypes.h"

#include <vulkan/vulkan.h>

#include <utility>

namespace acm::vulkan
{
	// Move-only RAII owner of an off-screen platform window that backs a headless
	// surface. Destroys the native window on reset/destruction, so no caller ever
	// makes a manual teardown call. Empty (valid() == false) when the surface needed
	// no backing window (a true windowless surface). The native handle is stored
	// type-erased so this header stays free of <windows.h>.
	class PlatformWindow
	{
	public:
		PlatformWindow() = default;
		explicit PlatformWindow(void* handle)
			: m_handle(handle)
		{
		}
		~PlatformWindow() { reset(); }

		PlatformWindow(PlatformWindow&& other) noexcept
			: m_handle(std::exchange(other.m_handle, nullptr))
		{
		}
		PlatformWindow& operator=(PlatformWindow&& other) noexcept
		{
			if (this != &other)
			{
				reset();
				m_handle = std::exchange(other.m_handle, nullptr);
			}
			return *this;
		}
		PlatformWindow(const PlatformWindow&) = delete;
		PlatformWindow& operator=(const PlatformWindow&) = delete;

		// Destroys the native window; a no-op when empty.
		void reset();
		bool valid() const { return m_handle != nullptr; }

	private:
		void* m_handle{nullptr};
	};

	// The outcome of a headless-surface attempt: the VkSurfaceKHR (VK_NULL_HANDLE on
	// failure) plus, when a backing window was created, its RAII owner. Ownership is
	// split deliberately — the caller destroys the surface (it holds the VkInstance
	// vkDestroySurfaceKHR needs), while the window is owned here and must outlive it.
	struct HeadlessSurface
	{
		VkSurfaceKHR surface{VK_NULL_HANDLE};
		PlatformWindow window;
	};

	// Creates a surface for offscreen ("headless") rendering, hiding the per-platform
	// mechanism. Prefers VK_EXT_headless_surface (a true windowless surface, e.g.
	// MoltenVK / lavapipe); where that extension is absent it falls back to a platform
	// window that is created but never shown (Windows desktop ICDs lack the headless
	// extension). The returned surface is VK_NULL_HANDLE when no mechanism is available.
	//
	// `extent` sizes the backing window on platforms that need one; it is unused where a
	// true windowless surface exists (there the swapchain's own desired extent drives the
	// render size). A zero dimension is treated as 1.
	HeadlessSurface createHeadlessSurface(VkInstance instance, [[maybe_unused]] acm::Extent2D extent);
} // namespace acm::vulkan
