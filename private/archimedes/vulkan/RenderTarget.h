#pragma once

#include "archimedes/acmTexture.h"
#include "archimedes/acmTypes.h"
#include "archimedes/HandleMap.h"
#include "archimedes/vulkan/Memory.h"

#include <vulkan/vulkan.h>

namespace acm::vulkan
{
	class Device;
	class Texture;

	class RenderTarget : public acm::ResourceSlot<acm::vulkan::RenderTarget, acm::vulkan::Device>
	{
	public:
		bool create(acm::vulkan::Device& owner, VkImage image, acm::Format format, acm::Extent2D extent, bool depth, acm::SampleCount samples);
		bool create(acm::vulkan::Device& owner, const acm::Texture& texture, acm::RenderTargetFinish finish, bool depth, acm::SampleCount samples);
		acm::Extent2D extent(const acm::Handle& handle) const;
		bool hasDepth(const acm::Handle& handle) const;
		bool multisampled(const acm::Handle& handle) const;
		VkSampleCountFlagBits sampleCount(const acm::Handle& handle) const { return accessible(handle) ? m_samples : VK_SAMPLE_COUNT_1_BIT; }
		VkFormat colorFormat(const acm::Handle& handle) const;
		VkFormat depthFormat(const acm::Handle& handle) const;
		bool beginRendering(const acm::Handle& handle, VkCommandBuffer commandBuffer, float r, float g, float b, float a) const;
		void endRendering(const acm::Handle& handle, VkCommandBuffer commandBuffer) const;
		void retire(acm::vulkan::Device& owner);

	private:
		struct Attachment
		{
			VkImage image{VK_NULL_HANDLE};
			acm::vulkan::Allocation allocation;
			VkImageView view{VK_NULL_HANDLE};
		};

		bool createTransientAttachments(acm::vulkan::Device& owner);
		bool createAttachment(acm::vulkan::Device& owner, Attachment& attachment, VkFormat format, acm::Extent2D extent, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageAspectFlags aspect);
		VkImage colorAttachmentImage() const;
		VkImageView colorAttachmentView() const;
		VkImage depthAttachmentImage() const;
		VkImageView depthAttachmentView() const;
		static VkImageMemoryBarrier2 imageBarrier(VkImage image, VkImageAspectFlags aspect, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage);
		void recordBeginTransitions(VkCommandBuffer commandBuffer) const;
		void recordEndTransition(VkCommandBuffer commandBuffer) const;
		static void retireAttachment(acm::vulkan::Device& owner, const Attachment& attachment);

		acm::Texture m_texture;
		acm::Texture m_depthTexture;
		VkImage m_colorImage{VK_NULL_HANDLE};
		VkImageView m_colorView{VK_NULL_HANDLE};
		bool m_ownsColorView{false};
		VkFormat m_colorFormat{VK_FORMAT_UNDEFINED};
		VkFormat m_depthFormat{VK_FORMAT_UNDEFINED};
		VkImageLayout m_finalColorLayout{VK_IMAGE_LAYOUT_UNDEFINED};
		VkAccessFlags2 m_finalColorAccess{VK_ACCESS_2_NONE};
		VkPipelineStageFlags2 m_finalColorStage{VK_PIPELINE_STAGE_2_NONE};
		bool m_depth{false};
		VkSampleCountFlagBits m_samples{VK_SAMPLE_COUNT_1_BIT};
		acm::Extent2D m_extent;
		Attachment m_msaaColor;
		Attachment m_msaaDepth;
	};
} // namespace acm::vulkan
