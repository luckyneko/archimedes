#pragma once

#include "archimedes/HandleMap.h"

#include <vulkan/vulkan.h>

namespace acm::vulkan
{
	class Device;

	class CommandPool : public acm::ResourceSlot<acm::vulkan::CommandPool, acm::vulkan::Device>
	{
	public:
		bool create(acm::vulkan::Device& owner);
		VkCommandPool vkCommandPool(const acm::Handle& handle) const;
		void retire(acm::vulkan::Device& owner);

	private:
		VkCommandPool m_pool{VK_NULL_HANDLE};
	};
} // namespace acm::vulkan
