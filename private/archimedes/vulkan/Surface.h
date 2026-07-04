/*
 *  Created by LuckyNeko on 24/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmGPU.h"
#include "archimedes/vulkan/PlatformSurface.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace acm::vulkan
{
	class Instance;

	// Move-only VkSurfaceKHR owner plus per-device support snapshot.
	class Surface
	{
	public:
		// Lifetime
		Surface() = default;
		// `window` is the optional off-screen window backing a headless surface (from
		// createHeadlessSurface); the Surface destroys it after the VkSurfaceKHR. Empty
		// for surfaces created from a caller-owned window.
		Surface(acm::vulkan::Instance& owner, VkSurfaceKHR surface, PlatformWindow window = {});
		~Surface();
		Surface(const Surface&) = delete;
		Surface& operator=(const Surface&) = delete;
		Surface(Surface&& other) noexcept;
		Surface& operator=(Surface&& other) noexcept;

		// State
		acm::vulkan::Instance& owner() const { return *m_owner; }
		bool valid() const { return m_owner && m_surface != VK_NULL_HANDLE && m_error.ok(); }
		acm::Error error() const { return m_error; }

		// Capabilities
		const std::vector<acm::SurfaceDeviceSupport>& support() const;
		VkSurfaceKHR vkSurface() const;

	private:
		// Internals
		void release();

		acm::vulkan::Instance* m_owner{nullptr};
		VkSurfaceKHR m_surface{VK_NULL_HANDLE};
		PlatformWindow m_window;
		std::vector<acm::SurfaceDeviceSupport> m_deviceSupport;
		acm::Error m_error;
	};
} // namespace acm::vulkan
