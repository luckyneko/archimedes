#pragma once

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
		bool create(acm::vulkan::Device& owner, VkRenderPass renderPass, VkImage image, acm::Format format, acm::Extent2D extent, bool depth, acm::SampleCount samples);
		bool create(acm::vulkan::Device& owner, acm::vulkan::Texture& texture, const acm::Handle& textureHandle, acm::RenderTargetFinish finish, bool depth, acm::SampleCount samples);
		acm::Extent2D extent(const acm::Handle& handle) const;
		bool hasDepth(const acm::Handle& handle) const;
		bool multisampled(const acm::Handle& handle) const;
		VkRenderPass vkRenderPass(const acm::Handle& handle) const;
		VkFramebuffer vkFramebuffer(const acm::Handle& handle) const;
		void beginRenderPass(const acm::Handle& handle, VkCommandBuffer commandBuffer, float r, float g, float b, float a) const;
		void retire(acm::vulkan::Device& owner);

	private:
		struct Attachment
		{
			VkImage image{VK_NULL_HANDLE};
			acm::vulkan::Allocation allocation;
			VkImageView view{VK_NULL_HANDLE};
		};

		bool createAttachment(acm::vulkan::Device& owner, Attachment& attachment, VkFormat format, acm::Extent2D extent, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageAspectFlags aspect);
		bool createFramebuffer(acm::vulkan::Device& owner, VkImageView colorView);
		VkRenderPass createOffscreenRenderPass(acm::vulkan::Device& owner, VkFormat colorFormat, acm::RenderTargetFinish finish, VkFormat depthFormat, VkSampleCountFlagBits samples);
		static void retireAttachment(acm::vulkan::Device& owner, const Attachment& attachment);

		acm::vulkan::Texture* m_textureResource{nullptr};
		acm::Handle m_texture;
		acm::vulkan::Texture* m_depthTextureResource{nullptr};
		acm::Handle m_depthTexture;
		VkRenderPass m_renderPass{VK_NULL_HANDLE};
		bool m_ownsRenderPass{false};
		bool m_depth{false};
		bool m_multisampled{false};
		VkImageView m_imageView{VK_NULL_HANDLE};
		VkFramebuffer m_framebuffer{VK_NULL_HANDLE};
		acm::Extent2D m_extent;
		Attachment m_msaaColor;
		Attachment m_msaaDepth;
	};
} // namespace acm::vulkan
