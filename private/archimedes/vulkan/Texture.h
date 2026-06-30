#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmTypes.h"
#include "archimedes/vulkan/Memory.h"

#include <vulkan/vulkan.h>

#include <cstdint>

namespace acm::vulkan
{
	class Buffer;
	class Device;

	class Texture
	{
	public:
		bool create(acm::vulkan::Device& owner, acm::Format format, acm::Extent2D extent, bool mipmapped, bool storage);
		acm::vulkan::Device& owner() const { return *m_owner; }
		acm::Format format() const;
		acm::Extent2D extent() const;
		uint32_t mipLevels() const;
		VkImage vkImage() const;
		VkImageView vkImageView() const;
		void recordCopyToBuffer(VkCommandBuffer commandBuffer, const acm::vulkan::Buffer& buffer) const;
		void recordTransition(VkCommandBuffer commandBuffer, acm::ImageLayout from, acm::ImageLayout to) const;
		acm::Error upload(const void* pixels, size_t size);
		void retire(acm::vulkan::Device& owner);

	private:
		static bool isDepthFormat(acm::Format format);
		static uint32_t computeMipLevels(acm::Extent2D extent);
		static void transition(VkCommandBuffer commandBuffer, VkImage image, uint32_t baseMip, uint32_t levelCount, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags2 sourceAccess, VkAccessFlags2 destinationAccess, VkPipelineStageFlags2 sourceStage, VkPipelineStageFlags2 destinationStage);

		acm::vulkan::Device* m_owner{nullptr};
		acm::Format m_format{acm::Format::Undefined};
		acm::Extent2D m_extent;
		uint32_t m_mipLevels{1};
		VkImage m_image{VK_NULL_HANDLE};
		acm::vulkan::Allocation m_allocation;
		VkImageView m_imageView{VK_NULL_HANDLE};
	};
} // namespace acm::vulkan
