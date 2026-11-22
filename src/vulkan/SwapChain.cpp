/*
 *  Created by LuckyNeko on 24/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/vulkan/SwapChain.h"

#include "archimedes/acmRenderTarget.h"
#include "archimedes/Log.h"
#include "archimedes/vulkan/Convert.h"
#include "archimedes/vulkan/Device.h"
#include "archimedes/vulkan/Instance.h"
#include "archimedes/vulkan/Surface.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace acm::vulkan
{

	// -----------------------------------------------------------------------------
	// Lifetime
	// -----------------------------------------------------------------------------

	SwapChain::SwapChain(Device& owner, const acm::Surface& surface, acm::SurfaceFormat format, acm::PresentMode presentMode, const acm::SwapChainConfig& config)
	{
		if (!surface.valid() || !surface.backend() || &surface.backend()->owner() != &owner.instance())
		{
			m_error = acm::Error("failed to create swapchain from invalid surface");
			return;
		}
		m_owner = &owner;
		m_surface = surface;
		m_format = format;
		m_presentMode = presentMode;
		m_config = config;
		// Reported once here rather than by each image's render target on every rebuild:
		// the clamp depends only on the device.
		const uint32_t samples = uint32_t(owner.sampleCount(config.samples));
		if (samples < sampleCountValue(config.samples))
			acm::logWarning("swapchain: " + std::to_string(sampleCountValue(config.samples)) + "x MSAA is not supported by this device; its render targets use " + std::to_string(samples) + "x");
		if (!rebuild(owner))
			m_error = acm::Error("failed to create swapchain");
	}

	SwapChain::~SwapChain()
	{
		release();
	}

	SwapChain::SwapChain(SwapChain&& other) noexcept
	{
		*this = std::move(other);
	}

	SwapChain& SwapChain::operator=(SwapChain&& other) noexcept
	{
		if (this == &other)
			return *this;
		release();
		m_owner = std::exchange(other.m_owner, nullptr);
		m_surface = std::move(other.m_surface);
		m_format = std::exchange(other.m_format, {});
		m_presentMode = std::exchange(other.m_presentMode, acm::PresentMode::Fifo);
		m_config = std::exchange(other.m_config, {});
		m_swapChain = std::exchange(other.m_swapChain, VK_NULL_HANDLE);
		m_extent = std::exchange(other.m_extent, {});
		m_renderTargets = std::move(other.m_renderTargets);
		m_generation = std::exchange(other.m_generation, 0);
		m_error = std::move(other.m_error);
		return *this;
	}

	// -----------------------------------------------------------------------------
	// Images
	// -----------------------------------------------------------------------------

	bool SwapChain::recreate()
	{
		// Other renderers may submit again as soon as the wait returns, so collection stays
		// bounded by the serial the wait completed.
		const uint64_t completedSerial = owner().waitIdle();
		if (!rebuild(owner()))
			return false;
		owner().collectGarbage(completedSerial);
		return true;
	}

	acm::SurfaceFormat SwapChain::format() const
	{
		return m_format;
	}

	acm::Extent2D SwapChain::extent() const
	{
		return m_extent;
	}

	size_t SwapChain::renderTargetCount() const
	{
		return m_renderTargets.size();
	}

	acm::RenderTarget SwapChain::renderTarget(size_t index) const
	{
		if (index >= m_renderTargets.size())
			return {};
		return m_renderTargets[index];
	}

	VkResult SwapChain::acquireNextImage(VkSemaphore semaphore, uint32_t& imageIndex) const
	{
		return vkAcquireNextImageKHR(owner().vkDevice(), m_swapChain, std::numeric_limits<uint64_t>::max(), semaphore, VK_NULL_HANDLE, &imageIndex);
	}

	VkSwapchainKHR SwapChain::vkSwapChain() const
	{
		return m_swapChain;
	}

	// -----------------------------------------------------------------------------
	// Internals
	// -----------------------------------------------------------------------------

	void SwapChain::release()
	{
		Device* owner = std::exchange(m_owner, nullptr);
		const VkSwapchainKHR swapChain = std::exchange(m_swapChain, VK_NULL_HANDLE);
		if (owner)
		{
			// Every render into and present of these images must finish before their views
			// and the swapchain go, and no submission serial covers a present.
			if (swapChain || !m_renderTargets.empty())
				owner->waitIdle();
			retireTargets(*owner);
			if (swapChain)
				vkDestroySwapchainKHR(owner->vkDevice(), swapChain, nullptr);
		}
		else
		{
			m_renderTargets.clear();
		}
		m_surface.reset();
		m_extent = {};
	}

	bool SwapChain::rebuild(Device& owner)
	{
		const VkSurfaceKHR surface = m_surface.valid() ? m_surface.backend()->vkSurface() : VK_NULL_HANDLE;
		if (!surface)
			return false;
		VkSurfaceCapabilitiesKHR capabilities = {};
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(owner.vkPhysicalDevice(), surface, &capabilities);
		VkExtent2D extent;
		if (capabilities.currentExtent.width != UINT32_MAX)
			extent = capabilities.currentExtent;
		else
		{
			extent.width = std::clamp(m_config.extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			extent.height = std::clamp(m_config.extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
		}
		if (extent.width == 0 || extent.height == 0)
			return false;

		uint32_t imageCount = capabilities.minImageCount + 1;
		if (capabilities.maxImageCount > 0)
			imageCount = std::min(imageCount, capabilities.maxImageCount);
		const VkSurfaceFormatKHR format{toVk(m_format.format), toVk(m_format.colorSpace)};
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
		createInfo.presentMode = toVk(m_presentMode);
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
			acm::RenderTarget target = owner.createRenderTarget(image, m_format.format, {extent.width, extent.height}, m_config.depth, m_config.samples);
			if (!target.valid())
			{
				// Nothing has used the new swapchain yet, so it and its views can go now.
				for (acm::RenderTarget& created : targets)
					owner.retireSwapChainTarget(created);
				vkDestroySwapchainKHR(owner.vkDevice(), newSwapChain, nullptr);
				return false;
			}
			targets.push_back(std::move(target));
		}

		// The old swapchain's views are destroyed before the swapchain itself, rather than
		// left to deferred collection: a sweep tags them with the newest serial, which another
		// renderer's submission can push past the wait the caller completed.
		const VkSwapchainKHR oldSwapChain = std::exchange(m_swapChain, newSwapChain);
		retireTargets(owner);
		if (oldSwapChain)
			vkDestroySwapchainKHR(owner.vkDevice(), oldSwapChain, nullptr);
		m_extent = {extent.width, extent.height};
		m_renderTargets = std::move(targets);
		++m_generation;
		return true;
	}

	void SwapChain::retireTargets(Device& owner)
	{
		for (acm::RenderTarget& target : m_renderTargets)
			owner.retireSwapChainTarget(target);
		m_renderTargets.clear();
	}

} // namespace acm::vulkan
