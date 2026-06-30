#pragma once

#include <vulkan/vulkan.h>

namespace acm::vulkan
{
	class Device;

	class CommandPool
	{
	public:
		bool create(acm::vulkan::Device& owner);
		acm::vulkan::Device& owner() const { return *m_owner; }
		VkCommandPool vkCommandPool() const;
		void retire(acm::vulkan::Device& owner);

	private:
		acm::vulkan::Device* m_owner{nullptr};
		VkCommandPool m_pool{VK_NULL_HANDLE};
	};
} // namespace acm::vulkan
