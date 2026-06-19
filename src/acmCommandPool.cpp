#include "archimedes/acmCommandPool.h"
#include "archimedes/acmCommandBuffer.h"
#include "archimedes/acmDevice.h"
#include <vulkan/vulkan.h>

struct acm::CommandPool::impl
{
	acm::Device device;
	VkCommandPool pool{VK_NULL_HANDLE};

	~impl()
	{
		// vkDestroyCommandPool frees every command buffer allocated from it, so
		// CommandBuffer has nothing of its own to destroy.
		if (pool && device.valid())
		{
			VkDevice dev = device.vkDevice();
			VkCommandPool p = pool;
			device.enqueueDestroy([dev, p]
								  { vkDestroyCommandPool(dev, p, nullptr); });
		}
	}
};

acm::CommandPool::CommandPool(acm::Device device)
	: m()
{
	auto impl = std::make_shared<acm::CommandPool::impl>();
	impl->device = device;

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = device.getQueueIdx();

	if (vkCreateCommandPool(impl->device.vkDevice(), &poolInfo, nullptr, &impl->pool) != VK_SUCCESS)
	{
		m_error = acm::Error("failed to create command pool");
		return;
	}

	m = impl;
}

acm::CommandBuffer acm::CommandPool::allocate()
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m->pool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer cb = VK_NULL_HANDLE;
	if (vkAllocateCommandBuffers(m->device.vkDevice(), &allocInfo, &cb) != VK_SUCCESS)
		return acm::CommandBuffer();

	return acm::CommandBuffer(*this, cb);
}

VkCommandPool acm::CommandPool::vkCommandPool() const
{
	return m->pool;
}
