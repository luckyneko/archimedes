#pragma once

#include "archimedes/acmGPU.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace acm::vulkan
{
	class Instance;

	class Surface
	{
	public:
		bool create(acm::vulkan::Instance& owner, VkSurfaceKHR surface);
		acm::vulkan::Instance& owner() const { return *m_owner; }
		const std::vector<acm::GPUSurfaceSupport>& support() const;
		VkSurfaceKHR vkSurface() const;
		void retire(acm::vulkan::Instance& owner);

	private:
		acm::vulkan::Instance* m_owner{nullptr};
		VkSurfaceKHR m_surface{VK_NULL_HANDLE};
		std::vector<acm::GPUSurfaceSupport> m_gpuSupport;
	};
} // namespace acm::vulkan
