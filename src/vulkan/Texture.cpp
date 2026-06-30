#include "archimedes/vulkan/Texture.h"

#include "archimedes/acmBuffer.h"
#include "archimedes/vulkan/Buffer.h"
#include "archimedes/vulkan/Convert.h"
#include "archimedes/vulkan/Device.h"

#include <algorithm>
#include <cassert>

bool acm::vulkan::Texture::create(acm::vulkan::Device& owner, acm::Format format, acm::Extent2D extent, bool mipmapped, bool storage)
{
	if (format == acm::Format::Undefined || extent.width == 0 || extent.height == 0)
		return false;
	m_owner = &owner;
	m_format = format;
	m_extent = extent;
	const VkFormat vkFormat = acm::vulkan::toVk(format);
	const bool depth = isDepthFormat(format);
	const VkImageAspectFlags aspect = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	m_mipLevels = (mipmapped && !depth) ? computeMipLevels(extent) : 1;

	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = vkFormat;
	imageInfo.extent = {extent.width, extent.height, 1};
	imageInfo.mipLevels = m_mipLevels;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = depth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
							: (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	if (storage && !depth)
		imageInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if (vkCreateImage(owner.vkDevice(), &imageInfo, nullptr, &m_image) != VK_SUCCESS)
		return false;

	VkMemoryRequirements requirements;
	vkGetImageMemoryRequirements(owner.vkDevice(), m_image, &requirements);
	m_allocation = owner.allocator().allocate(requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (!m_allocation.valid())
		return false;
	vkBindImageMemory(owner.vkDevice(), m_image, m_allocation.memory, m_allocation.offset);

	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = m_image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = vkFormat;
	viewInfo.subresourceRange.aspectMask = aspect;
	viewInfo.subresourceRange.levelCount = m_mipLevels;
	viewInfo.subresourceRange.layerCount = 1;
	return vkCreateImageView(owner.vkDevice(), &viewInfo, nullptr, &m_imageView) == VK_SUCCESS;
}

bool acm::vulkan::Texture::isDepthFormat(acm::Format format)
{
	return format == acm::Format::D32_Sfloat || format == acm::Format::D24_Unorm_S8_Uint;
}

uint32_t acm::vulkan::Texture::computeMipLevels(acm::Extent2D extent)
{
	uint32_t levels = 1;
	uint32_t dimension = std::max(extent.width, extent.height);
	while (dimension > 1)
	{
		dimension >>= 1;
		++levels;
	}
	return levels;
}

acm::Format acm::vulkan::Texture::format() const
{
	return m_format;
}

acm::Extent2D acm::vulkan::Texture::extent() const
{
	return m_extent;
}

uint32_t acm::vulkan::Texture::mipLevels() const
{
	return m_mipLevels;
}

VkImage acm::vulkan::Texture::vkImage() const
{
	return m_image;
}

VkImageView acm::vulkan::Texture::vkImageView() const
{
	return m_imageView;
}

void acm::vulkan::Texture::recordCopyToBuffer(VkCommandBuffer commandBuffer, const acm::vulkan::Buffer& buffer) const
{
	if (&owner() != &buffer.owner())
		return;
	const VkBuffer vkBuffer = buffer.vkBuffer();
	if (!vkBuffer)
		return;
	VkBufferImageCopy region = {};
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageExtent = {m_extent.width, m_extent.height, 1};
	vkCmdCopyImageToBuffer(commandBuffer, m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vkBuffer, 1, &region);
}

void acm::vulkan::Texture::recordTransition(VkCommandBuffer commandBuffer, acm::ImageLayout from, acm::ImageLayout to) const
{
	const acm::vulkan::VkLayoutInfo source = acm::vulkan::toVk(from);
	const acm::vulkan::VkLayoutInfo destination = acm::vulkan::toVk(to);
	VkImageMemoryBarrier2 barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	barrier.srcStageMask = source.stage;
	barrier.srcAccessMask = source.access;
	barrier.dstStageMask = destination.stage;
	barrier.dstAccessMask = destination.access;
	barrier.oldLayout = source.layout;
	barrier.newLayout = destination.layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = m_image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = m_mipLevels;
	barrier.subresourceRange.layerCount = 1;
	VkDependencyInfo dependency = {};
	dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependency.imageMemoryBarrierCount = 1;
	dependency.pImageMemoryBarriers = &barrier;
	vkCmdPipelineBarrier2(commandBuffer, &dependency);
}

acm::Error acm::vulkan::Texture::upload(const void* pixels, size_t size)
{
	assert(m_format != acm::Format::D32_Sfloat && "acm::Texture::upload: depth textures cannot be uploaded");

	uint32_t uploadedMipLevels = m_mipLevels;
	if (uploadedMipLevels > 1)
	{
		VkFormatProperties2 properties = {};
		properties.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
		vkGetPhysicalDeviceFormatProperties2(owner().vkPhysicalDevice(), acm::vulkan::toVk(m_format), &properties);
		if (!(properties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
			uploadedMipLevels = 1;
	}

	acm::Buffer staging = owner().createBuffer(size, acm::BufferUsage::Staging);
	if (!staging.valid())
		return acm::Error("Texture::upload: failed to create staging buffer");
	if (acm::Error error = staging.write(pixels, size))
		return error;

	const VkBuffer stagingBuffer = staging.native()->vkBuffer();
	const VkImage image = m_image;
	const acm::Extent2D textureExtent = m_extent;
	return owner().submitOneShot([stagingBuffer, image, textureExtent, uploadedMipLevels](VkCommandBuffer commandBuffer)
								 {
		transition(commandBuffer, image, 0, uploadedMipLevels, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_2_NONE, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE, VK_PIPELINE_STAGE_2_TRANSFER_BIT);

		VkBufferImageCopy region = {};
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.layerCount = 1;
		region.imageExtent = {textureExtent.width, textureExtent.height, 1};
		vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		int32_t width = int32_t(textureExtent.width);
		int32_t height = int32_t(textureExtent.height);
		for (uint32_t mip = 1; mip < uploadedMipLevels; ++mip)
		{
			transition(commandBuffer, image, mip - 1, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT);
			const int32_t nextWidth = width > 1 ? width / 2 : 1;
			const int32_t nextHeight = height > 1 ? height / 2 : 1;
			VkImageBlit blit = {};
			blit.srcOffsets[1] = {width, height, 1};
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = mip - 1;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[1] = {nextWidth, nextHeight, 1};
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = mip;
			blit.dstSubresource.layerCount = 1;
			vkCmdBlitImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
			transition(commandBuffer, image, mip - 1, 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_2_TRANSFER_READ_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
			width = nextWidth;
			height = nextHeight;
		}

		transition(commandBuffer, image, uploadedMipLevels - 1, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT); });
}

void acm::vulkan::Texture::transition(VkCommandBuffer commandBuffer, VkImage image, uint32_t baseMip, uint32_t levelCount, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags2 sourceAccess, VkAccessFlags2 destinationAccess, VkPipelineStageFlags2 sourceStage, VkPipelineStageFlags2 destinationStage)
{
	VkImageMemoryBarrier2 barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	barrier.srcStageMask = sourceStage;
	barrier.srcAccessMask = sourceAccess;
	barrier.dstStageMask = destinationStage;
	barrier.dstAccessMask = destinationAccess;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = baseMip;
	barrier.subresourceRange.levelCount = levelCount;
	barrier.subresourceRange.layerCount = 1;
	VkDependencyInfo dependency = {};
	dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependency.imageMemoryBarrierCount = 1;
	dependency.pImageMemoryBarriers = &barrier;
	vkCmdPipelineBarrier2(commandBuffer, &dependency);
}

void acm::vulkan::Texture::retire(acm::vulkan::Device& owner)
{
	const VkImageView imageView = std::exchange(m_imageView, VK_NULL_HANDLE);
	const VkImage image = std::exchange(m_image, VK_NULL_HANDLE);
	const acm::vulkan::Allocation allocation = std::exchange(m_allocation, {});
	m_format = acm::Format::Undefined;
	m_extent = {};
	m_mipLevels = 1;
	m_owner = nullptr;
	const VkDevice device = owner.vkDevice();
	if (imageView)
	{
		const VkImageView retiredView = imageView;
		owner.enqueueDestroy([device, retiredView]
							 { vkDestroyImageView(device, retiredView, nullptr); });
	}
	if (image)
	{
		const VkImage retiredImage = image;
		owner.enqueueDestroy([device, retiredImage]
							 { vkDestroyImage(device, retiredImage, nullptr); });
	}
	if (allocation.valid())
	{
		acm::vulkan::MemoryAllocator* allocator = &owner.allocator();
		const acm::vulkan::Allocation retiredAllocation = allocation;
		owner.enqueueDestroy([allocator, retiredAllocation]
							 { allocator->free(retiredAllocation); });
	}
}
