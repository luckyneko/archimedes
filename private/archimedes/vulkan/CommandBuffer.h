/*
 *  Created by LuckyNeko on 24/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmCommandPool.h"
#include "archimedes/acmError.h"
#include "archimedes/acmRenderTarget.h"
#include "archimedes/acmTypes.h"

#include <vulkan/vulkan.h>

namespace acm::vulkan
{
	class Device;
	class Buffer;
	class CommandPool;
	class ComputePipeline;
	class DescriptorSet;
	class Pipeline;
	class RenderTarget;
	class Texture;

	// Move-only owner of a primary VkCommandBuffer allocated from an acm::CommandPool.
	// It retains the public pool wrapper so vkFreeCommandBuffers has a live pool.
	class CommandBuffer
	{
	public:
		// Lifetime
		CommandBuffer() = default;
		CommandBuffer(acm::vulkan::Device& owner, const acm::CommandPool& pool);
		~CommandBuffer();
		CommandBuffer(const CommandBuffer&) = delete;
		CommandBuffer& operator=(const CommandBuffer&) = delete;
		CommandBuffer(CommandBuffer&& other) noexcept;
		CommandBuffer& operator=(CommandBuffer&& other) noexcept;

		// State
		acm::vulkan::Device& owner() const { return *m_owner; }
		bool valid() const { return m_owner && m_commandBuffer != VK_NULL_HANDLE && m_error.ok(); }
		acm::Error error() const { return m_error; }
		VkCommandBuffer vkCommandBuffer() const;

		// Recording
		acm::Error begin();
		acm::Error end();

		// Rendering
		void beginRendering(const acm::RenderTarget& target, float r, float g, float b, float a);
		void endRendering();
		void setViewportAndScissor(acm::Extent2D extent);
		void bindPipeline(const acm::vulkan::Pipeline& pipeline);
		void bindDescriptorSet(const acm::vulkan::Pipeline& pipeline, const acm::vulkan::DescriptorSet& set, const uint32_t* dynamicOffset);
		void draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
		void bindVertexBuffer(const acm::vulkan::Buffer& buffer);
		void bindIndexBuffer(const acm::vulkan::Buffer& buffer);
		void drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);
		void copyTextureToBuffer(const acm::vulkan::Texture& texture, const acm::vulkan::Buffer& buffer);

		// Compute
		void bindComputePipeline(const acm::vulkan::ComputePipeline& pipeline);
		void bindComputeDescriptorSet(const acm::vulkan::ComputePipeline& pipeline, const acm::vulkan::DescriptorSet& set);
		void dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ);

		// Synchronization
		void bufferBarrier(const acm::vulkan::Buffer& buffer, acm::ShaderStage srcStage, acm::ShaderStage dstStage);
		void transitionImage(const acm::vulkan::Texture& texture, acm::ImageLayout from, acm::ImageLayout to);

	private:
		// Internals
		void release();

		acm::vulkan::Device* m_owner{nullptr};
		acm::CommandPool m_pool;
		acm::RenderTarget m_renderTarget;
		VkCommandBuffer m_commandBuffer{VK_NULL_HANDLE};
		acm::Error m_error;
	};
} // namespace acm::vulkan
