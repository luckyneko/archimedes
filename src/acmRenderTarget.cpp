#include "archimedes/acmRenderTarget.h"

#include "archimedes/acmDevice.h"
#include "archimedes/acmTexture.h"
#include "archimedes/acmVkConvert.h"
#include "archimedes/acmVkMemory.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace
{
	// The fixed depth format the renderer uses for its depth buffers. D32_SFLOAT is a
	// required depth-attachment format, so no capability query is needed.
	constexpr acm::Format kDepthFormat = acm::Format::D32_Sfloat;

	// Creates a device-local 2D attachment image + view (memory from the device's pool),
	// for the owned MSAA color/depth images a RenderTarget needs (the swapchain/texture
	// is the resolve target). Returns false (handles left null) on failure.
	bool createAttachmentImage(acm::Device device, VkFormat format, acm::Extent2D extent, VkSampleCountFlagBits samples,
							   VkImageUsageFlags usage, VkImageAspectFlags aspect, VkImage& outImage, acm::Allocation& outAllocation, VkImageView& outView)
	{
		VkDevice dev = device.vkDevice();

		VkImageCreateInfo imageInfo = {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = format;
		imageInfo.extent = {extent.width, extent.height, 1};
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = samples;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = usage;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		if (vkCreateImage(dev, &imageInfo, nullptr, &outImage) != VK_SUCCESS)
			return false;

		VkMemoryRequirements memReq;
		vkGetImageMemoryRequirements(dev, outImage, &memReq);
		outAllocation = device.memoryAllocator().allocate(memReq, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		if (!outAllocation.valid())
		{
			vkDestroyImage(dev, outImage, nullptr);
			outImage = VK_NULL_HANDLE;
			return false;
		}
		vkBindImageMemory(dev, outImage, outAllocation.memory, outAllocation.offset);

		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = outImage;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = aspect;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.layerCount = 1;
		if (vkCreateImageView(dev, &viewInfo, nullptr, &outView) != VK_SUCCESS)
			return false;
		return true;
	}

	// Render pass for an offscreen color target: clear, store, and end in a layout
	// chosen by `finish` (SHADER_READ_ONLY to sample, TRANSFER_SRC to copy) so the
	// result is usable with no manual barrier. The subpass->external dependency makes
	// the color writes visible to that following stage. A non-UNDEFINED `depthFormat`
	// adds a depth attachment (cleared, not stored). When `samples` > 1 the color
	// attachment is multisampled and a single-sampled *resolve* attachment is added
	// (at index 1) for the final result; depth is then multisampled too. VK_NULL_HANDLE
	// on failure.
	VkRenderPass createOffscreenRenderPass(VkDevice device, VkFormat colorFormat, acm::RenderTargetFinish finish, VkFormat depthFormat, VkSampleCountFlagBits samples)
	{
		const bool sampled = (finish == acm::RenderTargetFinish::Sampled);
		const bool depth = (depthFormat != VK_FORMAT_UNDEFINED);
		const bool msaa = (samples != VK_SAMPLE_COUNT_1_BIT);
		const VkImageLayout finalColor = sampled ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

		// Attachment layout: [color(0), (resolve(1) if msaa), (depth) ]. The depth index
		// depends on whether a resolve attachment sits at 1.
		std::vector<VkAttachmentDescription> attachments;

		VkAttachmentDescription color = {};
		color.format = colorFormat;
		color.samples = samples;
		color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color.storeOp = msaa ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE; // msaa: result is the resolve
		color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		color.finalLayout = msaa ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : finalColor;
		attachments.push_back(color);

		if (msaa)
		{
			VkAttachmentDescription resolve = {};
			resolve.format = colorFormat;
			resolve.samples = VK_SAMPLE_COUNT_1_BIT;
			resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // fully written by the resolve
			resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			resolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			resolve.finalLayout = finalColor;
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

		std::vector<VkSubpassDependency> dependencies;
		VkSubpassDependency colorDep = {};
		colorDep.srcSubpass = 0;
		colorDep.dstSubpass = VK_SUBPASS_EXTERNAL;
		colorDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		colorDep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		colorDep.dstStageMask = sampled ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT;
		colorDep.dstAccessMask = sampled ? VK_ACCESS_SHADER_READ_BIT : VK_ACCESS_TRANSFER_READ_BIT;
		dependencies.push_back(colorDep);

		if (depth)
		{
			VkSubpassDependency depthDep = {};
			depthDep.srcSubpass = VK_SUBPASS_EXTERNAL;
			depthDep.dstSubpass = 0;
			depthDep.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			depthDep.srcAccessMask = 0;
			depthDep.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			depthDep.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dependencies.push_back(depthDep);
		}

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = uint32_t(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = uint32_t(dependencies.size());
		renderPassInfo.pDependencies = dependencies.data();

		VkRenderPass renderPass = VK_NULL_HANDLE;
		if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
			return VK_NULL_HANDLE;
		return renderPass;
	}
} // namespace

struct acm::RenderTarget::impl
{
	acm::Device device;
	acm::Texture texture;	   // offscreen only: keeps the rendered-into texture (+ its view) alive
	acm::Texture depthTexture; // non-MSAA depth only: the owned single-sampled depth buffer
	VkRenderPass renderPass{VK_NULL_HANDLE};
	bool ownsRenderPass{false};
	bool depth{false};
	bool multisampled{false};
	VkImageView imageView{VK_NULL_HANDLE}; // swapchain path: the borrowed color image's view
	VkFramebuffer frameBuffer{VK_NULL_HANDLE};
	acm::Extent2D extent;

	// Owned multisampled attachment images (MSAA only). The single-sampled resolve
	// target is the swapchain image / the texture; these are the transient N-sample
	// color (and, with depth, depth) buffers the subpass renders into.
	VkImage msaaColorImage{VK_NULL_HANDLE};
	acm::Allocation msaaColorAllocation;
	VkImageView msaaColorView{VK_NULL_HANDLE};
	VkImage msaaDepthImage{VK_NULL_HANDLE};
	acm::Allocation msaaDepthAllocation;
	VkImageView msaaDepthView{VK_NULL_HANDLE};

	~impl()
	{
		if (!device.valid())
			return;

		// Defer teardown onto the device's frame-fenced queue; enqueue order is run
		// order: framebuffer -> views -> images -> free memory ranges -> (owned) render
		// pass. The depthTexture member tears itself down after this body runs (still
		// after the framebuffer, which is enqueued here first).
		VkDevice dev = device.vkDevice();
		auto destroy = [&](auto handle, auto fn)
		{
			if (handle)
			{
				auto h = handle;
				device.enqueueDestroy([dev, h, fn]
									  { fn(dev, h, nullptr); });
			}
		};
		auto freeAlloc = [&](const acm::Allocation& allocation)
		{
			if (allocation.valid())
			{
				auto* alloc = &device.memoryAllocator();
				acm::Allocation a = allocation;
				device.enqueueDestroy([alloc, a]
									  { alloc->free(a); });
			}
		};
		destroy(frameBuffer, vkDestroyFramebuffer);
		destroy(imageView, vkDestroyImageView);
		destroy(msaaColorView, vkDestroyImageView);
		destroy(msaaDepthView, vkDestroyImageView);
		destroy(msaaColorImage, vkDestroyImage);
		destroy(msaaDepthImage, vkDestroyImage);
		freeAlloc(msaaColorAllocation);
		freeAlloc(msaaDepthAllocation);
		if (ownsRenderPass)
			destroy(renderPass, vkDestroyRenderPass);
	}
};

acm::RenderTarget::RenderTarget(acm::Device device, VkRenderPass renderPass, VkImage image, acm::Format format, acm::Extent2D extent, bool depth, acm::SampleCount samples)
	: m()
{
	auto impl = std::make_shared<acm::RenderTarget::impl>();
	impl->device = device;
	impl->renderPass = renderPass;
	impl->extent = extent;
	impl->depth = depth;

	const VkSampleCountFlagBits vkSamples = acm::toVkSampleCount(samples, device.getGPU().device);
	impl->multisampled = (vkSamples != VK_SAMPLE_COUNT_1_BIT);
	const VkFormat colorVk = acm::toVk(format);

	// View over the borrowed swapchain image (the single-sampled color / resolve target).
	VkImageViewCreateInfo imageViewInfo = {};
	imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewInfo.image = image;
	imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewInfo.format = colorVk;
	imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageViewInfo.subresourceRange.levelCount = 1;
	imageViewInfo.subresourceRange.layerCount = 1;
	if (vkCreateImageView(impl->device.vkDevice(), &imageViewInfo, nullptr, &impl->imageView) != VK_SUCCESS)
	{
		m_error = acm::Error("failed to create image view");
		return;
	}

	// Owned depth + MSAA color images, matched to the shared render pass.
	if (depth)
	{
		if (impl->multisampled)
		{
			if (!createAttachmentImage(device, acm::toVk(kDepthFormat), extent, vkSamples, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
									   VK_IMAGE_ASPECT_DEPTH_BIT, impl->msaaDepthImage, impl->msaaDepthAllocation, impl->msaaDepthView))
				return;
		}
		else
		{
			impl->depthTexture = impl->device.createTexture(kDepthFormat, extent);
			if (!impl->depthTexture.valid())
				return;
		}
	}
	if (impl->multisampled)
	{
		if (!createAttachmentImage(device, colorVk, extent, vkSamples, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
								   VK_IMAGE_ASPECT_COLOR_BIT, impl->msaaColorImage, impl->msaaColorAllocation, impl->msaaColorView))
			return;
	}

	// Framebuffer attachments must match the render pass: [color, (resolve), (depth)].
	VkImageView attachments[3];
	uint32_t count = 0;
	if (impl->multisampled)
	{
		attachments[count++] = impl->msaaColorView;
		attachments[count++] = impl->imageView; // resolve = the swapchain image
		if (depth)
			attachments[count++] = impl->msaaDepthView;
	}
	else
	{
		attachments[count++] = impl->imageView;
		if (depth)
			attachments[count++] = impl->depthTexture.vkImageView();
	}

	VkFramebufferCreateInfo framebufferInfo = {};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = renderPass;
	framebufferInfo.attachmentCount = count;
	framebufferInfo.pAttachments = attachments;
	framebufferInfo.width = extent.width;
	framebufferInfo.height = extent.height;
	framebufferInfo.layers = 1;
	if (vkCreateFramebuffer(impl->device.vkDevice(), &framebufferInfo, nullptr, &impl->frameBuffer) != VK_SUCCESS)
	{
		m_error = acm::Error("failed to create framebuffer");
		return;
	}

	m = impl;
}

acm::RenderTarget::RenderTarget(acm::Device device, acm::Texture texture, acm::RenderTargetFinish finish, bool depth, acm::SampleCount samples)
	: m()
{
	if (!texture.valid())
		return;

	auto impl = std::make_shared<acm::RenderTarget::impl>();
	impl->device = device;
	impl->texture = texture;
	impl->extent = texture.getExtent();
	impl->depth = depth;

	const VkSampleCountFlagBits vkSamples = acm::toVkSampleCount(samples, device.getGPU().device);
	impl->multisampled = (vkSamples != VK_SAMPLE_COUNT_1_BIT);
	const VkFormat colorVk = acm::toVk(texture.format());
	const VkFormat depthVk = depth ? acm::toVk(kDepthFormat) : VK_FORMAT_UNDEFINED;

	if (depth)
	{
		if (impl->multisampled)
		{
			if (!createAttachmentImage(device, depthVk, impl->extent, vkSamples, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
									   VK_IMAGE_ASPECT_DEPTH_BIT, impl->msaaDepthImage, impl->msaaDepthAllocation, impl->msaaDepthView))
				return;
		}
		else
		{
			impl->depthTexture = impl->device.createTexture(kDepthFormat, impl->extent);
			if (!impl->depthTexture.valid())
				return;
		}
	}
	if (impl->multisampled)
	{
		if (!createAttachmentImage(device, colorVk, impl->extent, vkSamples, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
								   VK_IMAGE_ASPECT_COLOR_BIT, impl->msaaColorImage, impl->msaaColorAllocation, impl->msaaColorView))
			return;
	}

	impl->renderPass = createOffscreenRenderPass(impl->device.vkDevice(), colorVk, finish, depthVk, vkSamples);
	if (impl->renderPass == VK_NULL_HANDLE)
		return;
	impl->ownsRenderPass = true;

	// Framebuffer attachments: [color, (resolve = texture), (depth)]. In the non-MSAA
	// case the texture's own view is the color attachment.
	VkImageView attachments[3];
	uint32_t count = 0;
	if (impl->multisampled)
	{
		attachments[count++] = impl->msaaColorView;
		attachments[count++] = texture.vkImageView(); // resolve target
		if (depth)
			attachments[count++] = impl->msaaDepthView;
	}
	else
	{
		attachments[count++] = texture.vkImageView();
		if (depth)
			attachments[count++] = impl->depthTexture.vkImageView();
	}

	VkFramebufferCreateInfo framebufferInfo = {};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = impl->renderPass;
	framebufferInfo.attachmentCount = count;
	framebufferInfo.pAttachments = attachments;
	framebufferInfo.width = impl->extent.width;
	framebufferInfo.height = impl->extent.height;
	framebufferInfo.layers = 1;
	if (vkCreateFramebuffer(impl->device.vkDevice(), &framebufferInfo, nullptr, &impl->frameBuffer) != VK_SUCCESS)
	{
		m_error = acm::Error("failed to create framebuffer");
		return;
	}

	m = impl;
}

acm::Extent2D acm::RenderTarget::getExtent() const
{
	return m->extent;
}

bool acm::RenderTarget::hasDepth() const
{
	return m->depth;
}

bool acm::RenderTarget::isMultisampled() const
{
	return m->multisampled;
}

VkRenderPass acm::RenderTarget::vkRenderPass()
{
	return m->renderPass;
}

VkFramebuffer acm::RenderTarget::vkFramebuffer()
{
	return m->frameBuffer;
}
