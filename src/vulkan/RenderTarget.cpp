#include "archimedes/vulkan/RenderTarget.h"

#include "archimedes/acmTexture.h"
#include "archimedes/vulkan/Convert.h"
#include "archimedes/vulkan/Device.h"
#include "archimedes/vulkan/Texture.h"

#include <utility>
#include <vector>

bool acm::vulkan::RenderTarget::create(acm::vulkan::Device& owner, VkRenderPass renderPass, VkImage image, acm::Format format, acm::Extent2D extent, bool depth, acm::SampleCount samples)
{
	if (!renderPass || !image || format == acm::Format::Undefined || extent.width == 0 || extent.height == 0)
		return false;
	m_renderPass = renderPass;
	m_extent = extent;
	m_depth = depth;
	const VkSampleCountFlagBits vkSamples = owner.sampleCount(samples);
	m_multisampled = vkSamples != VK_SAMPLE_COUNT_1_BIT;
	const VkFormat colorFormat = acm::vulkan::toVk(format);

	VkImageViewCreateInfo imageViewInfo = {};
	imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewInfo.image = image;
	imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewInfo.format = colorFormat;
	imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageViewInfo.subresourceRange.levelCount = 1;
	imageViewInfo.subresourceRange.layerCount = 1;
	if (vkCreateImageView(owner.vkDevice(), &imageViewInfo, nullptr, &m_imageView) != VK_SUCCESS)
		return false;

	constexpr acm::Format DepthFormat = acm::Format::D32_Sfloat;
	if (m_depth)
	{
		if (m_multisampled)
		{
			if (!createAttachment(owner, m_msaaDepth, acm::vulkan::toVk(DepthFormat), extent, vkSamples, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT))
				return false;
		}
		else
		{
			acm::Texture depthTexture = owner.createTexture(DepthFormat, extent, false, false);
			if (!depthTexture.valid() || !depthTexture.native()->retain(depthTexture.handle()))
				return false;
			m_depthTextureResource = depthTexture.native();
			m_depthTexture = depthTexture.handle();
		}
	}
	if (m_multisampled && !createAttachment(owner, m_msaaColor, colorFormat, extent, vkSamples, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_ASPECT_COLOR_BIT))
		return false;
	return createFramebuffer(owner, m_imageView);
}

bool acm::vulkan::RenderTarget::create(acm::vulkan::Device& owner, acm::vulkan::Texture& texture, const acm::Handle& textureHandle, acm::RenderTargetFinish finish, bool depth, acm::SampleCount samples)
{
	if (&texture.owner() != &owner)
		return false;
	const acm::Format format = texture.format(textureHandle);
	const acm::Extent2D extent = texture.extent(textureHandle);
	const VkImageView colorView = texture.vkImageView(textureHandle);
	if (format == acm::Format::Undefined || extent.width == 0 || extent.height == 0 || !colorView || !texture.retain(textureHandle))
		return false;
	m_textureResource = &texture;
	m_texture = textureHandle;
	m_extent = extent;
	m_depth = depth;
	const VkSampleCountFlagBits vkSamples = owner.sampleCount(samples);
	m_multisampled = vkSamples != VK_SAMPLE_COUNT_1_BIT;
	const VkFormat colorFormat = acm::vulkan::toVk(format);
	constexpr acm::Format DepthFormat = acm::Format::D32_Sfloat;
	const VkFormat depthFormat = depth ? acm::vulkan::toVk(DepthFormat) : VK_FORMAT_UNDEFINED;

	if (m_depth)
	{
		if (m_multisampled)
		{
			if (!createAttachment(owner, m_msaaDepth, depthFormat, extent, vkSamples, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT))
				return false;
		}
		else
		{
			acm::Texture depthTexture = owner.createTexture(DepthFormat, extent, false, false);
			if (!depthTexture.valid() || !depthTexture.native()->retain(depthTexture.handle()))
				return false;
			m_depthTextureResource = depthTexture.native();
			m_depthTexture = depthTexture.handle();
		}
	}
	if (m_multisampled && !createAttachment(owner, m_msaaColor, colorFormat, extent, vkSamples, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_ASPECT_COLOR_BIT))
		return false;
	m_renderPass = createOffscreenRenderPass(owner, colorFormat, finish, depthFormat, vkSamples);
	m_ownsRenderPass = m_renderPass != VK_NULL_HANDLE;
	return m_renderPass && createFramebuffer(owner, colorView);
}

bool acm::vulkan::RenderTarget::createAttachment(acm::vulkan::Device& owner, Attachment& attachment, VkFormat format, acm::Extent2D extent, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageAspectFlags aspect)
{
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
	if (vkCreateImage(owner.vkDevice(), &imageInfo, nullptr, &attachment.image) != VK_SUCCESS)
		return false;

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(owner.vkDevice(), attachment.image, &memoryRequirements);
	attachment.allocation = owner.allocator().allocate(memoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (!attachment.allocation.valid() || vkBindImageMemory(owner.vkDevice(), attachment.image, attachment.allocation.memory, attachment.allocation.offset) != VK_SUCCESS)
		return false;

	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = attachment.image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspect;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.layerCount = 1;
	return vkCreateImageView(owner.vkDevice(), &viewInfo, nullptr, &attachment.view) == VK_SUCCESS;
}

bool acm::vulkan::RenderTarget::createFramebuffer(acm::vulkan::Device& owner, VkImageView colorView)
{
	VkImageView attachments[3];
	uint32_t attachmentCount = 0;
	if (m_multisampled)
	{
		attachments[attachmentCount++] = m_msaaColor.view;
		attachments[attachmentCount++] = colorView;
		if (m_depth)
			attachments[attachmentCount++] = m_msaaDepth.view;
	}
	else
	{
		attachments[attachmentCount++] = colorView;
		if (m_depth)
		{
			const VkImageView depthView = m_depthTextureResource ? m_depthTextureResource->vkImageView(m_depthTexture) : VK_NULL_HANDLE;
			if (!depthView)
				return false;
			attachments[attachmentCount++] = depthView;
		}
	}
	VkFramebufferCreateInfo framebufferInfo = {};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = m_renderPass;
	framebufferInfo.attachmentCount = attachmentCount;
	framebufferInfo.pAttachments = attachments;
	framebufferInfo.width = m_extent.width;
	framebufferInfo.height = m_extent.height;
	framebufferInfo.layers = 1;
	return vkCreateFramebuffer(owner.vkDevice(), &framebufferInfo, nullptr, &m_framebuffer) == VK_SUCCESS;
}

VkRenderPass acm::vulkan::RenderTarget::createOffscreenRenderPass(acm::vulkan::Device& owner, VkFormat colorFormat, acm::RenderTargetFinish finish, VkFormat depthFormat, VkSampleCountFlagBits samples)
{
	const bool sampled = finish == acm::RenderTargetFinish::Sampled;
	const bool depth = depthFormat != VK_FORMAT_UNDEFINED;
	const bool multisampled = samples != VK_SAMPLE_COUNT_1_BIT;
	const VkImageLayout finalColor = sampled ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	std::vector<VkAttachmentDescription> attachments;
	VkAttachmentDescription color = {};
	color.format = colorFormat;
	color.samples = samples;
	color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color.storeOp = multisampled ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
	color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color.finalLayout = multisampled ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : finalColor;
	attachments.push_back(color);
	if (multisampled)
	{
		VkAttachmentDescription resolve = {};
		resolve.format = colorFormat;
		resolve.samples = VK_SAMPLE_COUNT_1_BIT;
		resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
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
		VkAttachmentDescription depthDescription = {};
		depthDescription.format = depthFormat;
		depthDescription.samples = samples;
		depthDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthDescription.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments.push_back(depthDescription);
	}
	VkAttachmentReference colorReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	VkAttachmentReference resolveReference = {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	VkAttachmentReference depthReference = {depthIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorReference;
	subpass.pResolveAttachments = multisampled ? &resolveReference : nullptr;
	subpass.pDepthStencilAttachment = depth ? &depthReference : nullptr;
	std::vector<VkSubpassDependency> dependencies;
	VkSubpassDependency colorDependency = {};
	colorDependency.srcSubpass = 0;
	colorDependency.dstSubpass = VK_SUBPASS_EXTERNAL;
	colorDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	colorDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	colorDependency.dstStageMask = sampled ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT;
	colorDependency.dstAccessMask = sampled ? VK_ACCESS_SHADER_READ_BIT : VK_ACCESS_TRANSFER_READ_BIT;
	dependencies.push_back(colorDependency);
	if (depth)
	{
		VkSubpassDependency depthDependency = {};
		depthDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		depthDependency.dstSubpass = 0;
		depthDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		depthDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		depthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies.push_back(depthDependency);
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
	return vkCreateRenderPass(owner.vkDevice(), &renderPassInfo, nullptr, &renderPass) == VK_SUCCESS ? renderPass : VK_NULL_HANDLE;
}

acm::Extent2D acm::vulkan::RenderTarget::extent(const acm::Handle& handle) const
{
	return accessible(handle) ? m_extent : acm::Extent2D{};
}

bool acm::vulkan::RenderTarget::hasDepth(const acm::Handle& handle) const
{
	return accessible(handle) && m_depth;
}

bool acm::vulkan::RenderTarget::multisampled(const acm::Handle& handle) const
{
	return accessible(handle) && m_multisampled;
}

VkRenderPass acm::vulkan::RenderTarget::vkRenderPass(const acm::Handle& handle) const
{
	return accessible(handle) ? m_renderPass : VK_NULL_HANDLE;
}

VkFramebuffer acm::vulkan::RenderTarget::vkFramebuffer(const acm::Handle& handle) const
{
	return accessible(handle) ? m_framebuffer : VK_NULL_HANDLE;
}

void acm::vulkan::RenderTarget::beginRenderPass(const acm::Handle& handle, VkCommandBuffer commandBuffer, float r, float g, float b, float a) const
{
	if (!accessible(handle))
		return;
	VkClearValue clears[3] = {};
	clears[0].color = {{r, g, b, a}};
	uint32_t clearCount = 1;
	if (m_depth)
	{
		const uint32_t depthIndex = m_multisampled ? 2u : 1u;
		clears[depthIndex].depthStencil = {1.0f, 0};
		clearCount = depthIndex + 1;
	}
	VkRenderPassBeginInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = m_renderPass;
	renderPassInfo.framebuffer = m_framebuffer;
	renderPassInfo.renderArea.extent = {m_extent.width, m_extent.height};
	renderPassInfo.clearValueCount = clearCount;
	renderPassInfo.pClearValues = clears;
	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void acm::vulkan::RenderTarget::retire(acm::vulkan::Device& owner)
{
	const VkDevice device = owner.vkDevice();
	const VkFramebuffer framebuffer = std::exchange(m_framebuffer, VK_NULL_HANDLE);
	const VkImageView imageView = std::exchange(m_imageView, VK_NULL_HANDLE);
	const VkRenderPass renderPass = std::exchange(m_renderPass, VK_NULL_HANDLE);
	const bool ownsRenderPass = std::exchange(m_ownsRenderPass, false);
	if (framebuffer)
		owner.enqueueDestroy([device, framebuffer]
							 { vkDestroyFramebuffer(device, framebuffer, nullptr); });
	if (imageView)
		owner.enqueueDestroy([device, imageView]
							 { vkDestroyImageView(device, imageView, nullptr); });
	retireAttachment(owner, std::exchange(m_msaaColor, {}));
	retireAttachment(owner, std::exchange(m_msaaDepth, {}));
	if (ownsRenderPass && renderPass)
		owner.enqueueDestroy([device, renderPass]
							 { vkDestroyRenderPass(device, renderPass, nullptr); });
	if (m_depthTextureResource)
	{
		auto* depthTexture = std::exchange(m_depthTextureResource, nullptr);
		const acm::Handle depthTextureHandle = std::exchange(m_depthTexture, {});
		depthTexture->release(depthTextureHandle);
	}
	if (m_textureResource)
	{
		auto* texture = std::exchange(m_textureResource, nullptr);
		const acm::Handle textureHandle = std::exchange(m_texture, {});
		texture->release(textureHandle);
	}
	m_depth = false;
	m_multisampled = false;
	m_extent = {};
}

void acm::vulkan::RenderTarget::retireAttachment(acm::vulkan::Device& owner, const Attachment& attachment)
{
	const VkDevice device = owner.vkDevice();
	if (attachment.view)
		owner.enqueueDestroy([device, view = attachment.view]
							 { vkDestroyImageView(device, view, nullptr); });
	if (attachment.image)
		owner.enqueueDestroy([device, image = attachment.image]
							 { vkDestroyImage(device, image, nullptr); });
	if (attachment.allocation.valid())
	{
		acm::vulkan::MemoryAllocator* allocator = &owner.allocator();
		owner.enqueueDestroy([allocator, allocation = attachment.allocation]
							 { allocator->free(allocation); });
	}
}
