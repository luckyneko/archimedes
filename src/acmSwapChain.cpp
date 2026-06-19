#include "archimedes/acmSwapChain.h"
#include "acmVkConvert.h"
#include "archimedes/acmDevice.h"
#include "archimedes/acmRenderTarget.h"
#include "archimedes/acmSurface.h"
#include <algorithm>
#include <vector>
#include <vulkan/vulkan.h>

namespace
{
	// The fixed depth format the swapchain's depth buffers use, matched by the per-
	// image RenderTargets. D32_SFLOAT is a required depth-attachment format.
	constexpr acm::Format kDepthFormat = acm::Format::D32_Sfloat;

	// One render pass is shared by every target in the swapchain — they all have the
	// same attachment layout, so there is no reason to build N identical ones. Layout:
	// [color(0), (resolve(1) if msaa), (depth)]. A non-UNDEFINED `depthFormat` adds a
	// depth attachment (cleared, not stored); `samples` > 1 makes the color attachment
	// multisampled with a single-sampled resolve attachment (the presented image) at
	// index 1. Returns VK_NULL_HANDLE on failure.
	VkRenderPass createColorRenderPass(VkDevice device, VkFormat colorFormat, VkFormat depthFormat, VkSampleCountFlagBits samples)
	{
		const bool depth = (depthFormat != VK_FORMAT_UNDEFINED);
		const bool msaa = (samples != VK_SAMPLE_COUNT_1_BIT);

		std::vector<VkAttachmentDescription> attachments;

		VkAttachmentDescription color = {};
		color.format = colorFormat;
		color.samples = samples;
		color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color.storeOp = msaa ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
		color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		color.finalLayout = msaa ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		attachments.push_back(color);

		if (msaa)
		{
			VkAttachmentDescription resolve = {};
			resolve.format = colorFormat;
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
		if (depth)
		{
			VkAttachmentDescription depthDesc = {};
			depthDesc.format = depthFormat;
			depthDesc.samples = samples;
			depthDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			depthDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			depthDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			attachments.push_back(depthDesc);
		}

		VkAttachmentReference colorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
		VkAttachmentReference resolveRef = {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
		VkAttachmentReference depthRef = {depthIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef;
		subpass.pResolveAttachments = msaa ? &resolveRef : nullptr;
		subpass.pDepthStencilAttachment = depth ? &depthRef : nullptr;

		// One external->subpass dependency covering color (and, with depth, the depth
		// attachment's load/transition); barriers both attachments' first access.
		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		if (depth)
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
		if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
			return VK_NULL_HANDLE;
		return renderPass;
	}
} // namespace

struct acm::SwapChain::impl
{
	acm::Device device;
	acm::Surface surface; // kept for rebuild() on resize
	acm::SurfaceFormat format;
	acm::PresentMode presentMode;
	acm::Extent2D desiredExtent;					 // only consulted for surfaces that defer sizing
	bool depth{false};								 // whether targets carry a depth buffer (render pass matches)
	acm::SampleCount samples{acm::SampleCount::One}; // MSAA sample count (targets + pass match)
	VkSwapchainKHR swapChain{VK_NULL_HANDLE};
	VkRenderPass renderPass{VK_NULL_HANDLE}; // shared by every render target, kept across rebuilds
	acm::Extent2D extents;

	std::vector<acm::RenderTarget> renderTargets;

	// (Re)creates the VkSwapchainKHR + its render targets from the surface's
	// current size, retiring any previous ones. The render pass is preserved
	// (format is unchanged across a resize). Returns false on failure, including a
	// zero-sized surface (minimized window) — the caller should retry later.
	bool rebuild();

	~impl()
	{
		if (!device.valid())
			return;

		// Drop the render targets first so their framebuffers/views are enqueued
		// ahead of the render pass and swapchain they depend on; enqueue order is
		// run order, so the device's deferred queue replays them safely:
		// framebuffers/views -> render pass -> swapchain.
		renderTargets.clear();
		VkDevice dev = device.vkDevice();
		if (renderPass)
		{
			VkRenderPass rp = renderPass;
			device.enqueueDestroy([dev, rp]
								  { vkDestroyRenderPass(dev, rp, nullptr); });
		}
		if (swapChain)
		{
			VkSwapchainKHR sc = swapChain;
			device.enqueueDestroy([dev, sc]
								  { vkDestroySwapchainKHR(dev, sc, nullptr); });
		}
	}
};

bool acm::SwapChain::impl::rebuild()
{
	const VkSurfaceFormatKHR vkFormat{acm::detail::toVk(format.format), acm::detail::toVk(format.colorSpace)};
	const VkPresentModeKHR vkPresentMode = acm::detail::toVk(presentMode);

	// Re-query the authoritative capabilities (currentExtent tracks window resize).
	VkSurfaceCapabilitiesKHR capabilities{};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.getGPU().device, surface.vkSurface(), &capabilities);

	// A currentExtent of UINT32_MAX means the surface defers sizing to the app
	// (e.g. headless); clamp the requested extent. Otherwise the surface dictates.
	VkExtent2D vkExtents;
	if (capabilities.currentExtent.width != UINT32_MAX)
	{
		vkExtents = capabilities.currentExtent;
	}
	else
	{
		vkExtents.width = std::clamp(desiredExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		vkExtents.height = std::clamp(desiredExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
	}
	// A zero-sized surface (minimized window) cannot back a swapchain.
	if (vkExtents.width == 0 || vkExtents.height == 0)
		return false;

	uint32_t imageCount = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
		imageCount = capabilities.maxImageCount;

	VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = surface.vkSurface();
	swapChainCreateInfo.minImageCount = imageCount;
	swapChainCreateInfo.imageFormat = vkFormat.format;
	swapChainCreateInfo.imageColorSpace = vkFormat.colorSpace;
	swapChainCreateInfo.imageExtent = vkExtents;
	swapChainCreateInfo.imageArrayLayers = 1;
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // Assume queue support presentation
	swapChainCreateInfo.queueFamilyIndexCount = 0;
	swapChainCreateInfo.preTransform = capabilities.currentTransform;
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapChainCreateInfo.presentMode = vkPresentMode;
	swapChainCreateInfo.clipped = VK_TRUE;
	swapChainCreateInfo.oldSwapchain = swapChain; // reuse the retiring one (NULL on first build)

	VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
	if (vkCreateSwapchainKHR(device.vkDevice(), &swapChainCreateInfo, nullptr, &newSwapchain) != VK_SUCCESS)
		return false;

	std::vector<VkImage> vkImages;
	vkGetSwapchainImagesKHR(device.vkDevice(), newSwapchain, &imageCount, nullptr);
	vkImages.resize(imageCount);
	vkGetSwapchainImagesKHR(device.vkDevice(), newSwapchain, &imageCount, vkImages.data());

	std::vector<acm::RenderTarget> newTargets;
	newTargets.reserve(vkImages.size());
	for (auto& image : vkImages)
	{
		auto target = device.createRenderTarget(renderPass, image, format.format, {vkExtents.width, vkExtents.height}, depth, samples);
		if (!target.valid())
		{
			VkDevice dev = device.vkDevice();
			device.enqueueDestroy([dev, newSwapchain]
								  { vkDestroySwapchainKHR(dev, newSwapchain, nullptr); });
			return false;
		}
		newTargets.push_back(target);
	}

	// Commit: retire the previous targets + swapchain (enqueue, framebuffers/views
	// before the swapchain that backs them), then adopt the new ones.
	const VkSwapchainKHR oldSwapchain = swapChain;
	renderTargets.clear();
	if (oldSwapchain)
	{
		VkDevice dev = device.vkDevice();
		device.enqueueDestroy([dev, oldSwapchain]
							  { vkDestroySwapchainKHR(dev, oldSwapchain, nullptr); });
	}

	swapChain = newSwapchain;
	extents = {vkExtents.width, vkExtents.height};
	renderTargets = std::move(newTargets);
	return true;
}

acm::SwapChain::SwapChain(acm::Device device, acm::Surface surface, acm::SurfaceFormat format, acm::PresentMode presentMode, acm::Extent2D desiredExtent, bool depth, acm::SampleCount samples)
	: m()
{
	auto impl = std::make_shared<acm::SwapChain::impl>();
	impl->device = device;
	impl->surface = surface;
	impl->format = format;
	impl->presentMode = presentMode;
	impl->desiredExtent = desiredExtent;
	impl->depth = depth;
	impl->samples = samples;

	// One render pass shared by all targets (hoisted out of RenderTarget); kept for
	// the swapchain's whole life, including across rebuilds. Adds a depth attachment
	// when requested (and the per-image targets then each own a depth buffer), and a
	// multisampled color + resolve when samples > 1.
	const VkFormat depthVk = depth ? acm::detail::toVk(kDepthFormat) : VK_FORMAT_UNDEFINED;
	const VkSampleCountFlagBits vkSamples = acm::detail::toVkSampleCount(samples, impl->device.getGPU().device);
	impl->renderPass = createColorRenderPass(impl->device.vkDevice(), acm::detail::toVk(format.format), depthVk, vkSamples);
	if (impl->renderPass == VK_NULL_HANDLE)
	{
		m_error = acm::Error("failed to create render pass");
		return;
	}

	if (!impl->rebuild())
	{
		m_error = acm::Error("failed to create swapchain");
		return;
	}

	m = impl;
}

bool acm::SwapChain::recreate()
{
	// Wait until the device is idle so the in-flight swapchain + its targets are
	// safe to retire, then rebuild against the surface's current size.
	vkDeviceWaitIdle(m->device.vkDevice());
	if (!m->rebuild())
		return false; // e.g. minimized (zero extent) — the caller retries later

	// Idle above means every retired-resource frame has completed, so flush the
	// deferred queue now and keep old swapchains/targets from piling up on resizes.
	m->device.collectGarbage(m->device.currentFrame());
	return true;
}

VkSwapchainKHR acm::SwapChain::vkSwapChain()
{
	return m->swapChain;
}

VkRenderPass acm::SwapChain::vkRenderPass()
{
	return m->renderPass;
}

acm::SurfaceFormat acm::SwapChain::getFormat() const
{
	return m->format;
}

acm::Extent2D acm::SwapChain::getExtents() const
{
	return m->extents;
}

size_t acm::SwapChain::getRenderTargetCount() const
{
	return m->renderTargets.size();
}

acm::RenderTarget acm::SwapChain::getRenderTarget(size_t idx) const
{
	return m->renderTargets[idx];
}
