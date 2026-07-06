/*
 *  Created by LuckyNeko on 24/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/vulkan/CommandBuffer.h"

#include "archimedes/vulkan/Buffer.h"
#include "archimedes/vulkan/CommandPool.h"
#include "archimedes/vulkan/ComputePipeline.h"
#include "archimedes/vulkan/Convert.h"
#include "archimedes/vulkan/DescriptorSet.h"
#include "archimedes/vulkan/Device.h"
#include "archimedes/vulkan/Pipeline.h"
#include "archimedes/vulkan/RenderTarget.h"
#include "archimedes/vulkan/Texture.h"

#include <utility>

namespace acm::vulkan
{

	// -----------------------------------------------------------------------------
	// Lifetime
	// -----------------------------------------------------------------------------

	CommandBuffer::CommandBuffer(Device& owner, const acm::CommandPool& pool)
	{
		if (!pool.valid() || !pool.backend() || &pool.backend()->owner() != &owner)
		{
			m_error = acm::Error("failed to allocate command buffer from invalid pool");
			return;
		}
		m_owner = &owner;
		const VkCommandPool vkPool = pool.backend()->vkCommandPool();
		if (!vkPool)
		{
			m_error = acm::Error("failed to allocate command buffer from invalid pool");
			return;
		}
		VkCommandBufferAllocateInfo allocationInfo = {};
		allocationInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocationInfo.commandPool = vkPool;
		allocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocationInfo.commandBufferCount = 1;
		if (vkAllocateCommandBuffers(owner.vkDevice(), &allocationInfo, &m_commandBuffer) != VK_SUCCESS)
		{
			m_error = acm::Error("failed to allocate command buffer");
			return;
		}
		m_pool = pool;
	}

	CommandBuffer::~CommandBuffer()
	{
		release();
	}

	CommandBuffer::CommandBuffer(CommandBuffer&& other) noexcept
	{
		*this = std::move(other);
	}

	CommandBuffer& CommandBuffer::operator=(CommandBuffer&& other) noexcept
	{
		if (this == &other)
			return *this;
		release();
		m_owner = std::exchange(other.m_owner, nullptr);
		m_pool = std::move(other.m_pool);
		m_renderTarget = std::move(other.m_renderTarget);
		m_graphicsPipelineBound = std::exchange(other.m_graphicsPipelineBound, false);
		m_graphicsPipelineColorFormat = std::exchange(other.m_graphicsPipelineColorFormat, VK_FORMAT_UNDEFINED);
		m_graphicsPipelineDepthFormat = std::exchange(other.m_graphicsPipelineDepthFormat, VK_FORMAT_UNDEFINED);
		m_graphicsPipelineSampleCount = std::exchange(other.m_graphicsPipelineSampleCount, VK_SAMPLE_COUNT_1_BIT);
		m_commandBuffer = std::exchange(other.m_commandBuffer, VK_NULL_HANDLE);
		m_error = std::move(other.m_error);
		return *this;
	}

	// -----------------------------------------------------------------------------
	// State
	// -----------------------------------------------------------------------------

	VkCommandBuffer CommandBuffer::vkCommandBuffer() const
	{
		return m_commandBuffer;
	}

	// -----------------------------------------------------------------------------
	// Recording
	// -----------------------------------------------------------------------------

	acm::Error CommandBuffer::begin()
	{
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		if (vkBeginCommandBuffer(m_commandBuffer, &beginInfo) != VK_SUCCESS)
			return acm::Error("failed to begin command buffer");
		return {};
	}

	acm::Error CommandBuffer::end()
	{
		if (vkEndCommandBuffer(m_commandBuffer) != VK_SUCCESS)
			return acm::Error("failed to end command buffer");
		return {};
	}

	// -----------------------------------------------------------------------------
	// Rendering
	// -----------------------------------------------------------------------------

	acm::Error CommandBuffer::beginRendering(const acm::RenderTarget& target, float r, float g, float b, float a)
	{
		if (m_renderTarget.valid())
			return acm::Error("rendering scope already active");
		if (!target.valid() || !target.backend() || &owner() != &target.backend()->owner())
			return acm::Error("failed to begin rendering from invalid render target");
		if (m_graphicsPipelineBound && !graphicsPipelineCompatibleWith(*target.backend()))
			return acm::Error("bound graphics pipeline is incompatible with render target");
		if (!target.backend()->beginRendering(m_commandBuffer, r, g, b, a))
			return acm::Error("failed to begin rendering");
		m_renderTarget = target;
		return {};
	}

	void CommandBuffer::endRendering()
	{
		acm::RenderTarget target = std::move(m_renderTarget);
		m_renderTarget.reset();
		// A pipeline binding is scoped to its rendering pass: forget it here so a pipeline
		// bound for this target isn't judged against the next beginRendering's target (a
		// different one, e.g. an offscreen pre-pass followed by the swapchain pass, would
		// otherwise fail the compatibility check even though the caller rebinds in the next
		// scope). A draw without a rebind still trips the m_graphicsPipelineBound guard.
		forgetGraphicsPipeline();
		if (!target.valid() || !target.backend())
			return;
		target.backend()->endRendering(m_commandBuffer);
	}

	void CommandBuffer::setViewportAndScissor(acm::Extent2D extent)
	{
		VkViewport viewport = {};
		viewport.width = float(extent.width);
		viewport.height = float(extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(m_commandBuffer, 0, 1, &viewport);
		VkRect2D scissor = {};
		scissor.extent = {extent.width, extent.height};
		vkCmdSetScissor(m_commandBuffer, 0, 1, &scissor);
	}

	acm::Error CommandBuffer::bindPipeline(const Pipeline& pipeline)
	{
		if (&owner() != &pipeline.owner())
			return acm::Error("failed to bind graphics pipeline owned by another device");
		if (m_renderTarget.valid() && (!m_renderTarget.backend() || !pipeline.compatibleWith(*m_renderTarget.backend())))
		{
			forgetGraphicsPipeline();
			return acm::Error("graphics pipeline is incompatible with active render target");
		}
		const VkPipeline vkPipeline = pipeline.vkPipeline();
		if (vkPipeline)
		{
			vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline);
			rememberGraphicsPipeline(pipeline);
			return {};
		}
		forgetGraphicsPipeline();
		return acm::Error("invalid graphics pipeline");
	}

	void CommandBuffer::bindDescriptorSet(const Pipeline& pipeline, const DescriptorSet& set, const uint32_t* dynamicOffset)
	{
		if (&owner() != &pipeline.owner() || &owner() != &set.owner())
			return;
		const VkPipelineLayout layout = pipeline.vkLayout();
		const VkDescriptorSet descriptorSet = set.vkDescriptorSet();
		if (!layout || !descriptorSet)
			return;
		const uint32_t dynamicOffsetCount = dynamicOffset ? 1u : 0u;
		vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descriptorSet, dynamicOffsetCount, dynamicOffset);
	}

	acm::Error CommandBuffer::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
	{
		if (!m_renderTarget.valid() || !m_graphicsPipelineBound)
			return acm::Error("draw requires an active render target and a compatible graphics pipeline");
		vkCmdDraw(m_commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
		return {};
	}

	void CommandBuffer::bindVertexBuffer(const Buffer& buffer)
	{
		if (&owner() != &buffer.owner())
			return;
		const VkBuffer vkBuffer = buffer.vkBuffer();
		if (!vkBuffer)
			return;
		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(m_commandBuffer, 0, 1, &vkBuffer, &offset);
	}

	void CommandBuffer::bindIndexBuffer(const Buffer& buffer)
	{
		if (&owner() != &buffer.owner())
			return;
		const VkBuffer vkBuffer = buffer.vkBuffer();
		if (vkBuffer)
			vkCmdBindIndexBuffer(m_commandBuffer, vkBuffer, 0, VK_INDEX_TYPE_UINT32);
	}

	acm::Error CommandBuffer::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
	{
		if (!m_renderTarget.valid() || !m_graphicsPipelineBound)
			return acm::Error("indexed draw requires an active render target and a compatible graphics pipeline");
		vkCmdDrawIndexed(m_commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
		return {};
	}

	void CommandBuffer::copyTextureToBuffer(const Texture& texture, const Buffer& buffer)
	{
		if (&owner() != &texture.owner() || &owner() != &buffer.owner())
			return;
		texture.recordCopyToBuffer(m_commandBuffer, buffer);
	}

	// -----------------------------------------------------------------------------
	// Compute
	// -----------------------------------------------------------------------------

	void CommandBuffer::bindComputePipeline(const ComputePipeline& pipeline)
	{
		if (&owner() != &pipeline.owner())
			return;
		const VkPipeline vkPipeline = pipeline.vkPipeline();
		if (vkPipeline)
			vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vkPipeline);
	}

	void CommandBuffer::bindComputeDescriptorSet(const ComputePipeline& pipeline, const DescriptorSet& set)
	{
		if (&owner() != &pipeline.owner() || &owner() != &set.owner())
			return;
		const VkPipelineLayout layout = pipeline.vkLayout();
		const VkDescriptorSet descriptorSet = set.vkDescriptorSet();
		if (layout && descriptorSet)
			vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &descriptorSet, 0, nullptr);
	}

	void CommandBuffer::dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
	{
		vkCmdDispatch(m_commandBuffer, groupsX, groupsY, groupsZ);
	}

	// -----------------------------------------------------------------------------
	// Synchronization
	// -----------------------------------------------------------------------------

	void CommandBuffer::bufferBarrier(const Buffer& buffer, acm::ShaderStage srcStage, acm::ShaderStage dstStage)
	{
		if (&owner() != &buffer.owner())
			return;
		const VkBuffer vkBuffer = buffer.vkBuffer();
		if (!vkBuffer)
			return;
		VkBufferMemoryBarrier2 barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
		barrier.srcStageMask = toVkPipelineStage(srcStage);
		barrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
		barrier.dstStageMask = toVkPipelineStage(dstStage);
		barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.buffer = vkBuffer;
		barrier.size = VK_WHOLE_SIZE;
		VkDependencyInfo dependency = {};
		dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		dependency.bufferMemoryBarrierCount = 1;
		dependency.pBufferMemoryBarriers = &barrier;
		vkCmdPipelineBarrier2(m_commandBuffer, &dependency);
	}

	void CommandBuffer::transitionImage(const Texture& texture, acm::ImageLayout from, acm::ImageLayout to)
	{
		if (&owner() != &texture.owner())
			return;
		texture.recordTransition(m_commandBuffer, from, to);
	}

	// -----------------------------------------------------------------------------
	// Internals
	// -----------------------------------------------------------------------------

	bool CommandBuffer::graphicsPipelineCompatibleWith(const RenderTarget& target) const
	{
		return m_graphicsPipelineColorFormat == target.colorFormat() && m_graphicsPipelineDepthFormat == target.depthFormat() && m_graphicsPipelineSampleCount == target.sampleCount();
	}

	void CommandBuffer::rememberGraphicsPipeline(const Pipeline& pipeline)
	{
		m_graphicsPipelineBound = true;
		m_graphicsPipelineColorFormat = pipeline.colorFormat();
		m_graphicsPipelineDepthFormat = pipeline.depthFormat();
		m_graphicsPipelineSampleCount = pipeline.sampleCount();
	}

	void CommandBuffer::forgetGraphicsPipeline()
	{
		m_graphicsPipelineBound = false;
		m_graphicsPipelineColorFormat = VK_FORMAT_UNDEFINED;
		m_graphicsPipelineDepthFormat = VK_FORMAT_UNDEFINED;
		m_graphicsPipelineSampleCount = VK_SAMPLE_COUNT_1_BIT;
	}

	void CommandBuffer::release()
	{
		m_renderTarget.reset();
		forgetGraphicsPipeline();
		const VkCommandBuffer commandBuffer = std::exchange(m_commandBuffer, VK_NULL_HANDLE);
		const VkCommandPool commandPool = m_pool.valid() ? m_pool.backend()->vkCommandPool() : VK_NULL_HANDLE;
		Device* owner = std::exchange(m_owner, nullptr);
		if (owner && commandPool && commandBuffer)
			vkFreeCommandBuffers(owner->vkDevice(), commandPool, 1, &commandBuffer);
		m_pool.reset();
	}

} // namespace acm::vulkan
