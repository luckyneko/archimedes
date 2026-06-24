#pragma once

#include "archimedes/acmGPU.h"
#include "archimedes/HandleMap.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace acm::vulkan
{
	class Instance;

	class Surface : public acm::ResourceSlot<acm::vulkan::Surface, acm::vulkan::Instance>
	{
	public:
		bool create(acm::vulkan::Instance& owner, VkSurfaceKHR surface);
		const std::vector<acm::GPUSurfaceSupport>& support(const acm::Handle& handle) const;
		VkSurfaceKHR vkSurface(const acm::Handle& handle) const;
		void retire(acm::vulkan::Instance& owner);

	private:
		VkSurfaceKHR m_surface{VK_NULL_HANDLE};
		std::vector<acm::GPUSurfaceSupport> m_gpuSupport;
	};
} // namespace acm::vulkan
