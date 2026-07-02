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

#include <vulkan/vulkan.h>

#include <vector>

namespace acm::vulkan
{
	class Instance;

	// Move-only VkSurfaceKHR owner plus per-GPU support snapshot.
	class Surface
	{
	public:
		// Lifetime
		Surface() = default;
		Surface(acm::vulkan::Instance& owner, VkSurfaceKHR surface);
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
		const std::vector<acm::GPUSurfaceSupport>& support() const;
		VkSurfaceKHR vkSurface() const;

	private:
		// Internals
		void release();

		acm::vulkan::Instance* m_owner{nullptr};
		VkSurfaceKHR m_surface{VK_NULL_HANDLE};
		std::vector<acm::GPUSurfaceSupport> m_gpuSupport;
		acm::Error m_error;
	};
} // namespace acm::vulkan
