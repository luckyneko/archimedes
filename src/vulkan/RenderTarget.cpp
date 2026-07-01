#include "archimedes/vulkan/RenderTarget.h"

#include "archimedes/acmTexture.h"
#include "archimedes/vulkan/Convert.h"
#include "archimedes/vulkan/Device.h"
#include "archimedes/vulkan/Texture.h"

#include <utility>

acm::vulkan::RenderTarget::RenderTarget(acm::vulkan::Device& owner, VkImage image, acm::Format format, acm::Extent2D extent, bool depth, acm::SampleCount samples)
{
	if (!image || format == acm::Format::Undefined || extent.width == 0 || extent.height == 0)
	{
		m_error = acm::Error("failed to create render target from invalid image");
		return;
	}
	m_owner = &owner;
	m_colorImage = image;
	m_colorFormat = acm::vulkan::toVk(format);
	m_extent = extent;
	m_depth = depth;
	m_depthFormat = depth ? acm::vulkan::toVk(acm::Format::D32_Sfloat) : VK_FORMAT_UNDEFINED;
	m_samples = owner.sampleCount(samples);
	m_finalColorLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	m_finalColorAccess = VK_ACCESS_2_NONE;
	m_finalColorStage = VK_PIPELINE_STAGE_2_NONE;

	VkImageViewCreateInfo imageViewInfo = {};
	imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewInfo.image = image;
	imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewInfo.format = m_colorFormat;
	imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageViewInfo.subresourceRange.levelCount = 1;
	imageViewInfo.subresourceRange.layerCount = 1;
	if (vkCreateImageView(owner.vkDevice(), &imageViewInfo, nullptr, &m_colorView) != VK_SUCCESS)
	{
		m_error = acm::Error("failed to create render target image view");
		return;
	}
	m_ownsColorView = true;

	if (!createTransientAttachments(owner))
		m_error = acm::Error("failed to create render target transient attachments");
}

acm::vulkan::RenderTarget::RenderTarget(acm::vulkan::Device& owner, const acm::Texture& texture, acm::RenderTargetFinish finish, bool depth, acm::SampleCount samples)
{
	if (!texture.valid() || !texture.native() || &texture.native()->owner() != &owner)
	{
		m_error = acm::Error("failed to create render target from invalid texture");
		return;
	}
	m_owner = &owner;
	const acm::Format format = texture.native()->format();
	const acm::Extent2D extent = texture.native()->extent();
	const uint32_t mipLevels = texture.native()->mipLevels();
	const VkImage colorImage = texture.native()->vkImage();
	const VkImageView colorView = texture.native()->vkImageView();
	if (format == acm::Format::Undefined || format == acm::Format::D32_Sfloat || format == acm::Format::D24_Unorm_S8_Uint || extent.width == 0 || extent.height == 0 || mipLevels != 1 || !colorImage || !colorView)
	{
		m_error = acm::Error("failed to create render target from unsupported texture");
		return;
	}
	m_texture = texture;
	m_colorImage = colorImage;
	m_colorView = colorView;
	m_colorFormat = acm::vulkan::toVk(format);
	m_extent = extent;
	m_depth = depth;
	m_depthFormat = depth ? acm::vulkan::toVk(acm::Format::D32_Sfloat) : VK_FORMAT_UNDEFINED;
	m_samples = owner.sampleCount(samples);
	if (finish == acm::RenderTargetFinish::Sampled)
	{
		m_finalColorLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		m_finalColorAccess = VK_ACCESS_2_SHADER_READ_BIT;
		m_finalColorStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	}
	else
	{
		m_finalColorLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		m_finalColorAccess = VK_ACCESS_2_TRANSFER_READ_BIT;
		m_finalColorStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	}

	if (!createTransientAttachments(owner))
		m_error = acm::Error("failed to create render target transient attachments");
}

acm::vulkan::RenderTarget::~RenderTarget()
{
	release();
}

acm::vulkan::RenderTarget::RenderTarget(RenderTarget&& other) noexcept
{
	*this = std::move(other);
}

acm::vulkan::RenderTarget& acm::vulkan::RenderTarget::operator=(RenderTarget&& other) noexcept
{
	if (this == &other)
		return *this;
	release();
	m_owner = std::exchange(other.m_owner, nullptr);
	m_texture = std::move(other.m_texture);
	m_depthTexture = std::move(other.m_depthTexture);
	m_colorImage = std::exchange(other.m_colorImage, VK_NULL_HANDLE);
	m_colorView = std::exchange(other.m_colorView, VK_NULL_HANDLE);
	m_ownsColorView = std::exchange(other.m_ownsColorView, false);
	m_colorFormat = std::exchange(other.m_colorFormat, VK_FORMAT_UNDEFINED);
	m_depthFormat = std::exchange(other.m_depthFormat, VK_FORMAT_UNDEFINED);
	m_finalColorLayout = std::exchange(other.m_finalColorLayout, VK_IMAGE_LAYOUT_UNDEFINED);
	m_finalColorAccess = std::exchange(other.m_finalColorAccess, VK_ACCESS_2_NONE);
	m_finalColorStage = std::exchange(other.m_finalColorStage, VK_PIPELINE_STAGE_2_NONE);
	m_depth = std::exchange(other.m_depth, false);
	m_samples = std::exchange(other.m_samples, VK_SAMPLE_COUNT_1_BIT);
	m_extent = std::exchange(other.m_extent, {});
	m_msaaColor = std::exchange(other.m_msaaColor, {});
	m_msaaDepth = std::exchange(other.m_msaaDepth, {});
	m_error = std::move(other.m_error);
	return *this;
}

bool acm::vulkan::RenderTarget::createTransientAttachments(acm::vulkan::Device& owner)
{
	const bool multisampled = m_samples != VK_SAMPLE_COUNT_1_BIT;
	if (m_depth)
	{
		if (multisampled)
		{
			if (!createAttachment(owner, m_msaaDepth, m_depthFormat, m_extent, m_samples, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT))
				return false;
		}
		else
		{
			acm::Texture depthTexture = owner.createTexture(acm::Format::D32_Sfloat, m_extent, false, false);
			if (!depthTexture.valid())
				return false;
			m_depthTexture = depthTexture;
		}
	}
	if (multisampled && !createAttachment(owner, m_msaaColor, m_colorFormat, m_extent, m_samples, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_ASPECT_COLOR_BIT))
		return false;
	return true;
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

VkImage acm::vulkan::RenderTarget::colorAttachmentImage() const
{
	return m_samples != VK_SAMPLE_COUNT_1_BIT ? m_msaaColor.image : m_colorImage;
}

VkImageView acm::vulkan::RenderTarget::colorAttachmentView() const
{
	return m_samples != VK_SAMPLE_COUNT_1_BIT ? m_msaaColor.view : m_colorView;
}

VkImage acm::vulkan::RenderTarget::depthAttachmentImage() const
{
	if (!m_depth)
		return VK_NULL_HANDLE;
	return m_samples != VK_SAMPLE_COUNT_1_BIT ? m_msaaDepth.image : (m_depthTexture.valid() ? m_depthTexture.native()->vkImage() : VK_NULL_HANDLE);
}

VkImageView acm::vulkan::RenderTarget::depthAttachmentView() const
{
	if (!m_depth)
		return VK_NULL_HANDLE;
	return m_samples != VK_SAMPLE_COUNT_1_BIT ? m_msaaDepth.view : (m_depthTexture.valid() ? m_depthTexture.native()->vkImageView() : VK_NULL_HANDLE);
}

VkImageMemoryBarrier2 acm::vulkan::RenderTarget::imageBarrier(VkImage image, VkImageAspectFlags aspect, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage)
{
	VkImageMemoryBarrier2 barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	barrier.srcStageMask = srcStage;
	barrier.srcAccessMask = srcAccess;
	barrier.dstStageMask = dstStage;
	barrier.dstAccessMask = dstAccess;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = aspect;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;
	return barrier;
}

void acm::vulkan::RenderTarget::recordBeginTransitions(VkCommandBuffer commandBuffer) const
{
	VkImageMemoryBarrier2 barriers[3] = {};
	uint32_t barrierCount = 0;
	if (const VkImage image = colorAttachmentImage())
	{
		barriers[barrierCount++] = imageBarrier(image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_2_NONE, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
	}
	if (m_samples != VK_SAMPLE_COUNT_1_BIT && m_colorImage)
	{
		barriers[barrierCount++] = imageBarrier(m_colorImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_2_NONE, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
	}
	if (const VkImage image = depthAttachmentImage())
	{
		barriers[barrierCount++] = imageBarrier(image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_ACCESS_2_NONE, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT);
	}
	if (barrierCount == 0)
		return;
	VkDependencyInfo dependency = {};
	dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependency.imageMemoryBarrierCount = barrierCount;
	dependency.pImageMemoryBarriers = barriers;
	vkCmdPipelineBarrier2(commandBuffer, &dependency);
}

void acm::vulkan::RenderTarget::recordEndTransition(VkCommandBuffer commandBuffer) const
{
	if (!m_colorImage || m_finalColorLayout == VK_IMAGE_LAYOUT_UNDEFINED)
		return;
	VkImageMemoryBarrier2 barrier = imageBarrier(m_colorImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, m_finalColorLayout, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, m_finalColorAccess, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, m_finalColorStage);
	VkDependencyInfo dependency = {};
	dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependency.imageMemoryBarrierCount = 1;
	dependency.pImageMemoryBarriers = &barrier;
	vkCmdPipelineBarrier2(commandBuffer, &dependency);
}

acm::Extent2D acm::vulkan::RenderTarget::extent() const
{
	return m_extent;
}

bool acm::vulkan::RenderTarget::hasDepth() const
{
	return m_depth;
}

bool acm::vulkan::RenderTarget::multisampled() const
{
	return m_samples != VK_SAMPLE_COUNT_1_BIT;
}

VkFormat acm::vulkan::RenderTarget::colorFormat() const
{
	return m_colorFormat;
}

VkFormat acm::vulkan::RenderTarget::depthFormat() const
{
	return m_depthFormat;
}

bool acm::vulkan::RenderTarget::beginRendering(VkCommandBuffer commandBuffer, float r, float g, float b, float a) const
{
	if (!commandBuffer || !m_colorView || !m_colorImage)
		return false;
	const VkImageView colorView = colorAttachmentView();
	if (!colorView)
		return false;
	recordBeginTransitions(commandBuffer);

	VkRenderingAttachmentInfo colorAttachment = {};
	colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.imageView = colorView;
	colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = m_samples != VK_SAMPLE_COUNT_1_BIT ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.clearValue.color = {{r, g, b, a}};
	if (m_samples != VK_SAMPLE_COUNT_1_BIT)
	{
		colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
		colorAttachment.resolveImageView = m_colorView;
		colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}

	VkRenderingAttachmentInfo depthAttachment = {};
	if (m_depth)
	{
		const VkImageView depthView = depthAttachmentView();
		if (!depthView)
			return false;
		depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depthAttachment.imageView = depthView;
		depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.clearValue.depthStencil = {1.0f, 0};
	}

	VkRenderingInfo renderingInfo = {};
	renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.renderArea.extent = {m_extent.width, m_extent.height};
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &colorAttachment;
	renderingInfo.pDepthAttachment = m_depth ? &depthAttachment : nullptr;
	vkCmdBeginRendering(commandBuffer, &renderingInfo);
	return true;
}

void acm::vulkan::RenderTarget::endRendering(VkCommandBuffer commandBuffer) const
{
	if (!commandBuffer)
		return;
	vkCmdEndRendering(commandBuffer);
	recordEndTransition(commandBuffer);
}

void acm::vulkan::RenderTarget::release()
{
	acm::vulkan::Device* owner = std::exchange(m_owner, nullptr);
	const VkImageView colorView = std::exchange(m_colorView, VK_NULL_HANDLE);
	const bool ownsColorView = std::exchange(m_ownsColorView, false);
	if (owner && ownsColorView && colorView)
		vkDestroyImageView(owner->vkDevice(), colorView, nullptr);
	if (owner)
	{
		destroyAttachment(*owner, std::exchange(m_msaaColor, {}));
		destroyAttachment(*owner, std::exchange(m_msaaDepth, {}));
	}
	else
	{
		m_msaaColor = {};
		m_msaaDepth = {};
	}
	m_depthTexture.reset();
	m_texture.reset();
	m_colorImage = VK_NULL_HANDLE;
	m_colorFormat = VK_FORMAT_UNDEFINED;
	m_depthFormat = VK_FORMAT_UNDEFINED;
	m_finalColorLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	m_finalColorAccess = VK_ACCESS_2_NONE;
	m_finalColorStage = VK_PIPELINE_STAGE_2_NONE;
	m_depth = false;
	m_samples = VK_SAMPLE_COUNT_1_BIT;
	m_extent = {};
}

void acm::vulkan::RenderTarget::destroyAttachment(acm::vulkan::Device& owner, const Attachment& attachment)
{
	if (attachment.view)
		vkDestroyImageView(owner.vkDevice(), attachment.view, nullptr);
	if (attachment.image)
		vkDestroyImage(owner.vkDevice(), attachment.image, nullptr);
	if (attachment.allocation.valid())
		owner.allocator().free(attachment.allocation);
}
