#include "archimedes/vulkan/CommandPool.h"

#include "archimedes/vulkan/Device.h"

bool acm::vulkan::CommandPool::create(acm::vulkan::Device& owner)
{
	VkCommandPoolCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	createInfo.queueFamilyIndex = owner.queueIndex();
	return vkCreateCommandPool(owner.vkDevice(), &createInfo, nullptr, &m_pool) == VK_SUCCESS;
}

VkCommandPool acm::vulkan::CommandPool::vkCommandPool(const acm::Handle& handle) const
{
	return accessible(handle) ? m_pool : VK_NULL_HANDLE;
}

void acm::vulkan::CommandPool::retire(acm::vulkan::Device& owner)
{
	const VkCommandPool retiredPool = std::exchange(m_pool, VK_NULL_HANDLE);
	if (!retiredPool)
		return;
	const VkDevice device = owner.vkDevice();
	owner.enqueueDestroy([device, retiredPool]
						 { vkDestroyCommandPool(device, retiredPool, nullptr); });
}
