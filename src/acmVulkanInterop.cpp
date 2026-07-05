/*
 *  Created by LuckyNeko on 05/07/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/acmVulkanInterop.h"

#include "archimedes/acmCommandBuffer.h"
#include "archimedes/acmDevice.h"
#include "archimedes/acmInstance.h"
#include "archimedes/acmSurface.h"
#include "archimedes/acmSwapChain.h"
#include "archimedes/acmTexture.h"
#include "archimedes/backendAPI.h"
#include "archimedes/vulkan/Convert.h"

namespace acm::interop
{
	VkInstance instance(const acm::Instance& inst)
	{
		acm::backend::Instance* b = inst.backend();
		return b ? b->vulkanInstance() : VK_NULL_HANDLE;
	}

	VkPhysicalDevice physicalDevice(const acm::Device& dev)
	{
		acm::backend::Device* b = dev.backend();
		return b ? b->vkPhysicalDevice() : VK_NULL_HANDLE;
	}

	VkDevice device(const acm::Device& dev)
	{
		acm::backend::Device* b = dev.backend();
		return b ? b->vkDevice() : VK_NULL_HANDLE;
	}

	VkQueue queue(const acm::Device& dev)
	{
		acm::backend::Device* b = dev.backend();
		return b ? b->vkQueue() : VK_NULL_HANDLE;
	}

	acm::Surface createSurface(acm::Instance& inst, VkSurfaceKHR surface)
	{
		acm::backend::Instance* b = inst.backend();
		return b ? b->createVulkanSurface(surface) : acm::Surface{};
	}

	VkCommandBuffer commandBuffer(const acm::CommandBuffer& cmd)
	{
		acm::backend::CommandBuffer* b = cmd.backend();
		return b ? b->vkCommandBuffer() : VK_NULL_HANDLE;
	}

	VkImageView imageView(const acm::Texture& tex)
	{
		acm::backend::Texture* b = tex.backend();
		return b ? b->vkImageView() : VK_NULL_HANDLE;
	}

	VkFormat colorFormat(const acm::SwapChain& swapChain)
	{
		if (!swapChain.valid())
			return VK_FORMAT_UNDEFINED;
		return acm::vulkan::toVk(swapChain.format().format);
	}
} // namespace acm::interop
