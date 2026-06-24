#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmTypes.h"
#include "archimedes/HandleMap.h"
#include "archimedes/vulkan/Memory.h"

#include <vulkan/vulkan.h>

#include <cstdint>

namespace acm::vulkan
{
	class Buffer;
	class Device;

	class Texture : public acm::ResourceSlot<acm::vulkan::Texture, acm::vulkan::Device>
	{
	public:
		bool create(acm::vulkan::Device& owner, acm::Format format, acm::Extent2D extent, bool mipmapped, bool storage);
		acm::Format format(const acm::Handle& handle) const;
		acm::Extent2D extent(const acm::Handle& handle) const;
		uint32_t mipLevels(const acm::Handle& handle) const;
		VkImage vkImage(const acm::Handle& handle) const;
		VkImageView vkImageView(const acm::Handle& handle) const;
		void recordCopyToBuffer(const acm::Handle& handle, VkCommandBuffer commandBuffer, const acm::vulkan::Buffer& buffer, const acm::Handle& bufferHandle) const;
		void recordTransition(const acm::Handle& handle, VkCommandBuffer commandBuffer, acm::ImageLayout from, acm::ImageLayout to) const;
		acm::Error upload(const acm::Handle& handle, const void* pixels, size_t size);
		void retire(acm::vulkan::Device& owner);

	private:
		static bool isDepthFormat(acm::Format format);
		static uint32_t computeMipLevels(acm::Extent2D extent);
		static void transition(VkCommandBuffer commandBuffer, VkImage image, uint32_t baseMip, uint32_t levelCount, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags2 sourceAccess, VkAccessFlags2 destinationAccess, VkPipelineStageFlags2 sourceStage, VkPipelineStageFlags2 destinationStage);

		acm::Format m_format{acm::Format::Undefined};
		acm::Extent2D m_extent;
		uint32_t m_mipLevels{1};
		VkImage m_image{VK_NULL_HANDLE};
		acm::vulkan::Allocation m_allocation;
		VkImageView m_imageView{VK_NULL_HANDLE};
	};
} // namespace acm::vulkan
