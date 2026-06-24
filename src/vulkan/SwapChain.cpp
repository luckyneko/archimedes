#include "archimedes/vulkan/SwapChain.h"

#include "archimedes/acmRenderTarget.h"
#include "archimedes/vulkan/Convert.h"
#include "archimedes/vulkan/Device.h"
#include "archimedes/vulkan/Instance.h"
#include "archimedes/vulkan/Surface.h"

#include <algorithm>
#include <limits>
#include <utility>

bool acm::vulkan::SwapChain::create(acm::vulkan::Device& owner, acm::vulkan::Surface& surface, const acm::Handle& surfaceHandle, acm::SurfaceFormat format, acm::PresentMode presentMode, acm::Extent2D desiredExtent, bool depth, acm::SampleCount samples)
{
	if (&surface.owner() != &owner.instance() || !surface.retain(surfaceHandle))
		return false;
	m_surfaceResource = &surface;
	m_surface = surfaceHandle;
	m_format = format;
	m_presentMode = presentMode;
	m_desiredExtent = desiredExtent;
	m_depth = depth;
	m_samples = samples;
	m_renderPass = createRenderPass(owner);
	return m_renderPass && rebuild(owner);
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
	const VkSurfaceKHR surface = m_surfaceResource ? m_surfaceResource->vkSurface(m_surface) : VK_NULL_HANDLE;
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
		acm::RenderTarget target = owner.createRenderTarget(m_renderPass, image, m_format.format, {extent.width, extent.height}, m_depth, m_samples);
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

VkRenderPass acm::vulkan::SwapChain::createRenderPass(acm::vulkan::Device& owner) const
{
	const VkSampleCountFlagBits samples = acm::vulkan::toVkSampleCount(m_samples, owner.vkPhysicalDevice());
	const VkFormat depthFormat = m_depth ? acm::vulkan::toVk(acm::Format::D32_Sfloat) : VK_FORMAT_UNDEFINED;
	const bool multisampled = samples != VK_SAMPLE_COUNT_1_BIT;
	std::vector<VkAttachmentDescription> attachments;
	VkAttachmentDescription color = {};
	color.format = acm::vulkan::toVk(m_format.format);
	color.samples = samples;
	color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color.storeOp = multisampled ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
	color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color.finalLayout = multisampled ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	attachments.push_back(color);
	if (multisampled)
	{
		VkAttachmentDescription resolve = {};
		resolve.format = acm::vulkan::toVk(m_format.format);
		resolve.samples = VK_SAMPLE_COUNT_1_BIT;
		resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		resolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		resolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		attachments.push_back(resolve);
	}
	const uint32_t depthIndex = uint32_t(attachments.size());
	if (m_depth)
	{
		VkAttachmentDescription depth = {};
		depth.format = depthFormat;
		depth.samples = samples;
		depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments.push_back(depth);
	}
	VkAttachmentReference colorReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	VkAttachmentReference resolveReference = {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	VkAttachmentReference depthReference = {depthIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorReference;
	subpass.pResolveAttachments = multisampled ? &resolveReference : nullptr;
	subpass.pDepthStencilAttachment = m_depth ? &depthReference : nullptr;
	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	if (m_depth)
	{
		dependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		dependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	}
	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = uint32_t(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;
	VkRenderPass renderPass = VK_NULL_HANDLE;
	return vkCreateRenderPass(owner.vkDevice(), &renderPassInfo, nullptr, &renderPass) == VK_SUCCESS ? renderPass : VK_NULL_HANDLE;
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
	const VkRenderPass renderPass = std::exchange(m_renderPass, VK_NULL_HANDLE);
	const VkSwapchainKHR swapChain = std::exchange(m_swapChain, VK_NULL_HANDLE);
	if (renderPass)
		owner.enqueueDestroy([device, renderPass]
							 { vkDestroyRenderPass(device, renderPass, nullptr); });
	if (swapChain)
		owner.enqueueDestroy([device, swapChain]
							 { vkDestroySwapchainKHR(device, swapChain, nullptr); });
	if (m_surfaceResource)
	{
		auto* surface = std::exchange(m_surfaceResource, nullptr);
		const acm::Handle surfaceHandle = std::exchange(m_surface, {});
		owner.enqueueDestroy([surface, surfaceHandle]
							 { surface->release(surfaceHandle); });
	}
	m_extent = {};
}
