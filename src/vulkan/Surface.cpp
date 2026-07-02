/*
 *  Created by LuckyNeko on 24/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/vulkan/Surface.h"

#include "archimedes/vulkan/Convert.h"
#include "archimedes/vulkan/Instance.h"

#include <utility>

acm::vulkan::Surface::Surface(acm::vulkan::Instance& owner, VkSurfaceKHR surface)
{
	if (!surface)
	{
		m_error = acm::Error("failed to create surface from null handle");
		return;
	}
	m_owner = &owner;
	m_surface = surface;
	const auto& gpus = owner.gpus();
	m_gpuSupport.resize(gpus.size());
	for (size_t gpuIndex = 0; gpuIndex < gpus.size(); ++gpuIndex)
	{
		const acm::GPU& gpu = gpus[gpuIndex];
		const VkPhysicalDevice physicalDevice = owner.physicalDevice(uint32_t(gpuIndex));
		acm::GPUSurfaceSupport& support = m_gpuSupport[gpuIndex];
		support.gpuIndex = gpu.index;

		VkSurfaceCapabilitiesKHR capabilities = {};
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);
		support.capabilities.minImageCount = capabilities.minImageCount;
		support.capabilities.maxImageCount = capabilities.maxImageCount;
		support.capabilities.currentExtent = {capabilities.currentExtent.width, capabilities.currentExtent.height};
		support.capabilities.minImageExtent = {capabilities.minImageExtent.width, capabilities.minImageExtent.height};
		support.capabilities.maxImageExtent = {capabilities.maxImageExtent.width, capabilities.maxImageExtent.height};

		uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
		std::vector<VkSurfaceFormatKHR> formats(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());
		for (const VkSurfaceFormatKHR& format : formats)
		{
			acm::Format neutralFormat;
			acm::ColorSpace colorSpace;
			if (acm::vulkan::tryFromVk(format.format, neutralFormat) && acm::vulkan::tryFromVk(format.colorSpace, colorSpace))
				support.supportedFormats.push_back({neutralFormat, colorSpace});
		}

		uint32_t modeCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, nullptr);
		std::vector<VkPresentModeKHR> modes(modeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, modes.data());
		for (VkPresentModeKHR mode : modes)
		{
			acm::PresentMode neutralMode;
			if (acm::vulkan::tryFromVk(mode, neutralMode))
				support.supportedPresentModes.push_back(neutralMode);
		}

		support.queueFamilySupportsPresent.resize(gpu.queueFamilies.size());
		for (const acm::GPUQueueFamily& queue : gpu.queueFamilies)
		{
			VkBool32 present = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queue.index, surface, &present);
			support.queueFamilySupportsPresent[queue.index] = present == VK_TRUE;
		}
	}
}

acm::vulkan::Surface::~Surface()
{
	release();
}

acm::vulkan::Surface::Surface(Surface&& other) noexcept
{
	*this = std::move(other);
}

acm::vulkan::Surface& acm::vulkan::Surface::operator=(Surface&& other) noexcept
{
	if (this == &other)
		return *this;
	release();
	m_owner = std::exchange(other.m_owner, nullptr);
	m_surface = std::exchange(other.m_surface, VK_NULL_HANDLE);
	m_gpuSupport = std::move(other.m_gpuSupport);
	m_error = std::move(other.m_error);
	return *this;
}

const std::vector<acm::GPUSurfaceSupport>& acm::vulkan::Surface::support() const
{
	return m_gpuSupport;
}

VkSurfaceKHR acm::vulkan::Surface::vkSurface() const
{
	return m_surface;
}

void acm::vulkan::Surface::release()
{
	acm::vulkan::Instance* owner = std::exchange(m_owner, nullptr);
	const VkSurfaceKHR surface = std::exchange(m_surface, VK_NULL_HANDLE);
	m_gpuSupport.clear();
	if (owner && surface)
		vkDestroySurfaceKHR(owner->nativeInstance(), surface, nullptr);
}
