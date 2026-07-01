#include "archimedes/vulkan/SwapChain.h"

#include "archimedes/acmRenderTarget.h"
#include "archimedes/vulkan/Convert.h"
#include "archimedes/vulkan/Device.h"
#include "archimedes/vulkan/Instance.h"
#include "archimedes/vulkan/Surface.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

acm::vulkan::SwapChain::SwapChain(acm::vulkan::Device& owner, const acm::Surface& surface, acm::SurfaceFormat format, acm::PresentMode presentMode, acm::Extent2D desiredExtent, bool depth, acm::SampleCount samples)
{
	if (!surface.valid() || !surface.native() || &surface.native()->owner() != &owner.instance())
	{
		m_error = acm::Error("failed to create swapchain from invalid surface");
		return;
	}
	m_owner = &owner;
	m_surface = surface;
	m_format = format;
	m_presentMode = presentMode;
	m_desiredExtent = desiredExtent;
	m_depth = depth;
	m_samples = samples;
	if (!rebuild(owner))
		m_error = acm::Error("failed to create swapchain");
}

acm::vulkan::SwapChain::~SwapChain()
{
	release();
}

acm::vulkan::SwapChain::SwapChain(SwapChain&& other) noexcept
{
	*this = std::move(other);
}

acm::vulkan::SwapChain& acm::vulkan::SwapChain::operator=(SwapChain&& other) noexcept
{
	if (this == &other)
		return *this;
	release();
	m_owner = std::exchange(other.m_owner, nullptr);
	m_surface = std::move(other.m_surface);
	m_format = std::exchange(other.m_format, {});
	m_presentMode = std::exchange(other.m_presentMode, acm::PresentMode::Fifo);
	m_desiredExtent = std::exchange(other.m_desiredExtent, {});
	m_depth = std::exchange(other.m_depth, false);
	m_samples = std::exchange(other.m_samples, acm::SampleCount::One);
	m_swapChain = std::exchange(other.m_swapChain, VK_NULL_HANDLE);
	m_extent = std::exchange(other.m_extent, {});
	m_renderTargets = std::move(other.m_renderTargets);
	m_error = std::move(other.m_error);
	return *this;
}

bool acm::vulkan::SwapChain::recreate()
{
	owner().waitIdle();
	if (!rebuild(owner()))
		return false;
	owner().collectGarbage(std::numeric_limits<uint64_t>::max());
	return true;
}

bool acm::vulkan::SwapChain::rebuild(acm::vulkan::Device& owner)
{
	const VkSurfaceKHR surface = m_surface.valid() ? m_surface.native()->vkSurface() : VK_NULL_HANDLE;
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
				owner.invalidateRenderTarget(created);
			targets.clear();
			owner.waitIdle();
			vkDestroySwapchainKHR(owner.vkDevice(), newSwapChain, nullptr);
			return false;
		}
		targets.push_back(std::move(target));
	}

	const bool hadOldTargets = !m_renderTargets.empty();
	const VkSwapchainKHR oldSwapChain = std::exchange(m_swapChain, newSwapChain);
	if (hadOldTargets || oldSwapChain)
		owner.waitIdle();
	invalidateTargets(owner);
	if (hadOldTargets)
		owner.collectGarbage(std::numeric_limits<uint64_t>::max());
	if (oldSwapChain)
		vkDestroySwapchainKHR(owner.vkDevice(), oldSwapChain, nullptr);
	m_extent = {extent.width, extent.height};
	m_renderTargets = std::move(targets);
	return true;
}

acm::SurfaceFormat acm::vulkan::SwapChain::format() const
{
	return m_format;
}

acm::Extent2D acm::vulkan::SwapChain::extent() const
{
	return m_extent;
}

size_t acm::vulkan::SwapChain::renderTargetCount() const
{
	return m_renderTargets.size();
}

acm::RenderTarget acm::vulkan::SwapChain::renderTarget(size_t index) const
{
	if (index >= m_renderTargets.size())
		return {};
	return m_renderTargets[index];
}

VkResult acm::vulkan::SwapChain::acquireNextImage(VkSemaphore semaphore, uint32_t& imageIndex) const
{
	return vkAcquireNextImageKHR(owner().vkDevice(), m_swapChain, std::numeric_limits<uint64_t>::max(), semaphore, VK_NULL_HANDLE, &imageIndex);
}

VkSwapchainKHR acm::vulkan::SwapChain::vkSwapChain() const
{
	return m_swapChain;
}

void acm::vulkan::SwapChain::invalidateTargets(acm::vulkan::Device& owner)
{
	for (acm::RenderTarget& target : m_renderTargets)
		owner.invalidateRenderTarget(target);
	m_renderTargets.clear();
}

void acm::vulkan::SwapChain::release()
{
	acm::vulkan::Device* owner = std::exchange(m_owner, nullptr);
	if (owner)
	{
		owner->waitIdle();
		invalidateTargets(*owner);
		owner->collectGarbage(std::numeric_limits<uint64_t>::max());
	}
	else
	{
		m_renderTargets.clear();
	}
	const VkSwapchainKHR swapChain = std::exchange(m_swapChain, VK_NULL_HANDLE);
	if (owner && swapChain)
		vkDestroySwapchainKHR(owner->vkDevice(), swapChain, nullptr);
	m_surface.reset();
	m_extent = {};
}
