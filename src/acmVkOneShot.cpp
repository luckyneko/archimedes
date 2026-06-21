#include "archimedes/acmVkOneShot.h"

#include "archimedes/acmDevice.h"

namespace acm
{
	acm::Error oneShotSubmit(acm::Device device, const std::function<void(VkCommandBuffer)>& record)
	{
		VkDevice dev = device.vkDevice();

		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		poolInfo.queueFamilyIndex = device.getQueueIdx();
		VkCommandPool pool = VK_NULL_HANDLE;
		if (vkCreateCommandPool(dev, &poolInfo, nullptr, &pool) != VK_SUCCESS)
			return acm::Error("oneShotSubmit: failed to create command pool");

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = pool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;
		VkCommandBuffer cb = VK_NULL_HANDLE;
		vkAllocateCommandBuffers(dev, &allocInfo, &cb);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(cb, &beginInfo);
		record(cb);
		vkEndCommandBuffer(cb);

		VkSubmitInfo submit = {};
		submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &cb;
		vkQueueSubmit(device.vkQueue(), 1, &submit, VK_NULL_HANDLE);
		vkQueueWaitIdle(device.vkQueue());

		vkDestroyCommandPool(dev, pool, nullptr); // frees cb with it
		return acm::Error{};
	}
} // namespace acm
