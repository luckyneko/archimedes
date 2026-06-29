#include "archimedes/vulkan/SwapChain.h"

#include "archimedes/acmRenderTarget.h"
#include "archimedes/vulkan/Convert.h"
#include "archimedes/vulkan/Device.h"
#include "archimedes/vulkan/Instance.h"
#include "archimedes/vulkan/Surface.h"

#include <algorithm>
#include <limits>
#include <utility>

bool acm::vulkan::SwapChain::create(acm::vulkan::Device& owner, const acm::Surface& surface, acm::SurfaceFormat format, acm::PresentMode presentMode, acm::Extent2D desiredExtent, bool depth, acm::SampleCount samples)
{
	if (!surface.valid() || !surface.native() || &surface.native()->owner() != &owner.instance())
		return false;
	m_surface = surface;
	m_format = format;
	m_presentMode = presentMode;
	m_desiredExtent = desiredExtent;
	m_depth = depth;
	m_samples = samples;
	return rebuild(owner);
}

bool acm::vulkan::SwapChain::recreate(const acm::Handle& handle)
{
	if (!accessible(handle))
		return false;
	owner().waitIdle();
	if (!rebuild(owner()))
		return false;
	owner().collectGarbage(owner().currentFrame());
	return true;
}

bool acm::vulkan::SwapChain::rebuild(acm::vulkan::Device& owner)
{
	const VkSurfaceKHR surface = m_surface.valid() ? m_surface.native()->vkSurface(m_surface.handle()) : VK_NULL_HANDLE;
	if (!surface)
		return false;
	VkSurfaceCapabilitiesKHR capabilities = {};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(owner.vkPhysicalDevice(), surface, &capabilities);
	VkExtent2D extent;
	if (capabilities.currentExtent.width != UINT32_MAX)
		extent = capabilities.currentExtent;
	else
	{
		extent.width = std::clamp(m_desiredExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		extent.height = std::clamp(m_desiredExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
	}
	if (extent.width == 0 || extent.height == 0)
		return false;

	uint32_t imageCount = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount > 0)
		imageCount = std::min(imageCount, capabilities.maxImageCount);
	const VkSurfaceFormatKHR format{acm::vulkan::toVk(m_format.format), acm::vulkan::toVk(m_format.colorSpace)};
	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = format.format;
	createInfo.imageColorSpace = format.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.preTransform = capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = acm::vulkan::toVk(m_presentMode);
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = m_swapChain;

	VkSwapchainKHR newSwapChain = VK_NULL_HANDLE;
	if (vkCreateSwapchainKHR(owner.vkDevice(), &createInfo, nullptr, &newSwapChain) != VK_SUCCESS)
		return false;
	vkGetSwapchainImagesKHR(owner.vkDevice(), newSwapChain, &imageCount, nullptr);
	std::vector<VkImage> images(imageCount);
	vkGetSwapchainImagesKHR(owner.vkDevice(), newSwapChain, &imageCount, images.data());
	std::vector<acm::RenderTarget> targets;
	targets.reserve(images.size());
	for (VkImage image : images)
	{
		acm::RenderTarget target = owner.createRenderTarget(image, m_format.format, {extent.width, extent.height}, m_depth, m_samples);
		if (!target.valid())
		{
			for (acm::RenderTarget& created : targets)
				created.native()->forceInvalidate(created.handle());
			const VkDevice device = owner.vkDevice();
			owner.enqueueDestroy([device, newSwapChain]
								 { vkDestroySwapchainKHR(device, newSwapChain, nullptr); });
			return false;
		}
		targets.push_back(std::move(target));
	}

	const VkSwapchainKHR oldSwapChain = std::exchange(m_swapChain, newSwapChain);
	retireTargets(owner);
	if (oldSwapChain)
	{
		const VkDevice device = owner.vkDevice();
		owner.enqueueDestroy([device, oldSwapChain]
							 { vkDestroySwapchainKHR(device, oldSwapChain, nullptr); });
	}
	m_extent = {extent.width, extent.height};
	m_renderTargets = std::move(targets);
	return true;
}

acm::SurfaceFormat acm::vulkan::SwapChain::format(const acm::Handle& handle) const
{
	return accessible(handle) ? m_format : acm::SurfaceFormat{};
}

acm::Extent2D acm::vulkan::SwapChain::extent(const acm::Handle& handle) const
{
	return accessible(handle) ? m_extent : acm::Extent2D{};
}

size_t acm::vulkan::SwapChain::renderTargetCount(const acm::Handle& handle) const
{
	return accessible(handle) ? m_renderTargets.size() : 0;
}

acm::RenderTarget acm::vulkan::SwapChain::renderTarget(const acm::Handle& handle, size_t index) const
{
	if (!accessible(handle) || index >= m_renderTargets.size())
		return {};
	return m_renderTargets[index];
}

VkResult acm::vulkan::SwapChain::acquireNextImage(const acm::Handle& handle, VkSemaphore semaphore, uint32_t& imageIndex) const
{
	if (!accessible(handle))
		return VK_ERROR_OUT_OF_DATE_KHR;
	return vkAcquireNextImageKHR(owner().vkDevice(), m_swapChain, std::numeric_limits<uint64_t>::max(), semaphore, VK_NULL_HANDLE, &imageIndex);
}

VkSwapchainKHR acm::vulkan::SwapChain::vkSwapChain(const acm::Handle& handle) const
{
	return accessible(handle) ? m_swapChain : VK_NULL_HANDLE;
}

void acm::vulkan::SwapChain::retireTargets(acm::vulkan::Device& owner)
{
	for (acm::RenderTarget& target : m_renderTargets)
		target.native()->forceInvalidate(target.handle());
	m_renderTargets.clear();
}

void acm::vulkan::SwapChain::retire(acm::vulkan::Device& owner)
{
	retireTargets(owner);
	const VkDevice device = owner.vkDevice();
	const VkSwapchainKHR swapChain = std::exchange(m_swapChain, VK_NULL_HANDLE);
	if (swapChain)
		owner.enqueueDestroy([device, swapChain]
							 { vkDestroySwapchainKHR(device, swapChain, nullptr); });
	if (m_surface.valid())
	{
		acm::Surface surface = m_surface;
		m_surface.reset();
		owner.enqueueDestroy([surface]() mutable
							 { surface.reset(); });
	}
	m_extent = {};
}
