/*
 *  Created by LuckyNeko on 24/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmTexture.h"
#include "archimedes/acmTypes.h"
#include "archimedes/vulkan/Memory.h"

#include <vulkan/vulkan.h>

namespace acm::vulkan
{
	class Device;
	class Texture;

	class RenderTarget
	{
	public:
		RenderTarget() = default;
		RenderTarget(acm::vulkan::Device& owner, VkImage image, acm::Format format, acm::Extent2D extent, bool depth, acm::SampleCount samples);
		RenderTarget(acm::vulkan::Device& owner, const acm::Texture& texture, acm::RenderTargetFinish finish, bool depth, acm::SampleCount samples);
		~RenderTarget();
		RenderTarget(const RenderTarget&) = delete;
		RenderTarget& operator=(const RenderTarget&) = delete;
		RenderTarget(RenderTarget&& other) noexcept;
		RenderTarget& operator=(RenderTarget&& other) noexcept;

		acm::vulkan::Device& owner() const { return *m_owner; }
		bool valid() const { return m_owner && m_colorImage != VK_NULL_HANDLE && colorAttachmentView() != VK_NULL_HANDLE && m_error.ok(); }
		acm::Error error() const { return m_error; }
		acm::Extent2D extent() const;
		bool hasDepth() const;
		bool multisampled() const;
		VkSampleCountFlagBits sampleCount() const { return m_samples; }
		VkFormat colorFormat() const;
		VkFormat depthFormat() const;
		bool beginRendering(VkCommandBuffer commandBuffer, float r, float g, float b, float a) const;
		void endRendering(VkCommandBuffer commandBuffer) const;

	private:
		struct Attachment
		{
			VkImage image{VK_NULL_HANDLE};
			acm::vulkan::Allocation allocation;
			VkImageView view{VK_NULL_HANDLE};
		};

		void release();
		bool createTransientAttachments(acm::vulkan::Device& owner);
		bool createAttachment(acm::vulkan::Device& owner, Attachment& attachment, VkFormat format, acm::Extent2D extent, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageAspectFlags aspect);
		VkImage colorAttachmentImage() const;
		VkImageView colorAttachmentView() const;
		VkImage depthAttachmentImage() const;
		VkImageView depthAttachmentView() const;
		static VkImageMemoryBarrier2 imageBarrier(VkImage image, VkImageAspectFlags aspect, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage);
		void recordBeginTransitions(VkCommandBuffer commandBuffer) const;
		void recordEndTransition(VkCommandBuffer commandBuffer) const;
		static void destroyAttachment(acm::vulkan::Device& owner, const Attachment& attachment);

		acm::vulkan::Device* m_owner{nullptr};
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
		acm::Error m_error;
	};
} // namespace acm::vulkan
