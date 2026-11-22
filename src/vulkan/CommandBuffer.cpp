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

#include <string>
#include <utility>

namespace acm::vulkan
{
	namespace
	{
		bool isDepthFormat(acm::Format format)
		{
			return format == acm::Format::D32_Sfloat || format == acm::Format::D24_Unorm_S8_Uint;
		}

		acm::Error callError(const char* call, const char* reason)
		{
			return acm::Error(std::string(call) + ": " + reason);
		}

		// The error for a buffer whose usage lacks what `call` needs; `needs` says what.
		acm::Error usageError(const char* call, const Buffer& buffer, const char* needs)
		{
			return acm::Error(std::string(call) + ": a BufferUsage::" + bufferUsageName(buffer.usage()) + " buffer cannot be used as " + needs);
		}
	} // namespace

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
		m_recording = std::exchange(other.m_recording, false);
		m_renderingActive = std::exchange(other.m_renderingActive, false);
		m_graphicsPipelineBound = std::exchange(other.m_graphicsPipelineBound, false);
		m_computePipelineBound = std::exchange(other.m_computePipelineBound, false);
		m_graphicsPipelineColorFormat = std::exchange(other.m_graphicsPipelineColorFormat, VK_FORMAT_UNDEFINED);
		m_graphicsPipelineDepthFormat = std::exchange(other.m_graphicsPipelineDepthFormat, VK_FORMAT_UNDEFINED);
		m_graphicsPipelineSampleCount = std::exchange(other.m_graphicsPipelineSampleCount, VK_SAMPLE_COUNT_1_BIT);
		m_commandBuffer = std::exchange(other.m_commandBuffer, VK_NULL_HANDLE);
		m_recordingError = std::move(other.m_recordingError);
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
		resetRecordingState();
		// A recording abandoned without end() (a caller returning early on an error) is
		// still in the recording state, where vkBeginCommandBuffer is not allowed; reset it
		// first. The pool is created with RESET_COMMAND_BUFFER_BIT.
		if (std::exchange(m_recording, false) && vkResetCommandBuffer(m_commandBuffer, 0) != VK_SUCCESS)
			return fail("failed to reset abandoned command buffer recording");
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		if (vkBeginCommandBuffer(m_commandBuffer, &beginInfo) != VK_SUCCESS)
			return fail("failed to begin command buffer");
		m_recording = true;
		return {};
	}

	acm::Error CommandBuffer::end()
	{
		if (acm::Error error = requireRecording("end"))
			return error;
		// An open rendering scope would make vkEndCommandBuffer invalid: close it, and
		// report the missing endRendering().
		acm::Error scopeError;
		if (m_renderingActive)
		{
			scopeError = fail("end: a rendering scope is still active; call endRendering() first");
			endRendering();
		}
		m_recording = false;
		if (vkEndCommandBuffer(m_commandBuffer) != VK_SUCCESS)
			return fail("failed to end command buffer");
		return scopeError;
	}

	// -----------------------------------------------------------------------------
	// Rendering
	// -----------------------------------------------------------------------------

	acm::Error CommandBuffer::beginRendering(const acm::RenderTarget& target, float r, float g, float b, float a)
	{
		if (acm::Error error = requireRecording("beginRendering"))
			return error;
		if (m_renderingActive)
			return fail("rendering scope already active");
		const RenderTarget* backendTarget = target.backend();
		if (!backendTarget)
			return fail("failed to begin rendering from invalid render target");
		if (&owner() != &backendTarget->owner())
			return fail("failed to begin rendering: render target is owned by another device");
		if (m_graphicsPipelineBound && !graphicsPipelineCompatibleWith(*backendTarget))
			return fail("bound graphics pipeline is incompatible with render target");
		if (!backendTarget->beginRendering(m_commandBuffer, r, g, b, a))
			return fail("failed to begin rendering");
		m_renderTarget = target;
		m_renderingActive = true;
		return {};
	}

	acm::Error CommandBuffer::endRendering()
	{
		if (acm::Error error = requireRecording("endRendering"))
			return error;
		if (!m_renderingActive)
			return fail("endRendering: no rendering scope is active");
		acm::RenderTarget target = std::move(m_renderTarget);
		m_renderTarget.reset();
		m_renderingActive = false;
		// A pipeline binding is scoped to its rendering pass: forget it here so a pipeline
		// bound for this target isn't judged against the next beginRendering's target (a
		// different one, e.g. an offscreen pre-pass followed by the swapchain pass, would
		// otherwise fail the compatibility check even though the caller rebinds in the next
		// scope). A draw without a rebind still trips the m_graphicsPipelineBound guard.
		forgetGraphicsPipeline();
		if (const RenderTarget* backendTarget = target.backend())
		{
			backendTarget->endRendering(m_commandBuffer);
			return {};
		}
		// The target was retired mid-scope (swapchain recreation): still close the scope
		// so the buffer can end, but the recording is not usable.
		vkCmdEndRendering(m_commandBuffer);
		return fail("endRendering: the render target was invalidated while rendering");
	}

	acm::Error CommandBuffer::setViewportAndScissor(acm::Extent2D extent)
	{
		if (acm::Error error = requireRecording("setViewportAndScissor"))
			return error;
		if (extent.width == 0 || extent.height == 0)
			return fail("setViewportAndScissor: extent must be non-zero");
		VkViewport viewport = {};
		viewport.width = float(extent.width);
		viewport.height = float(extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(m_commandBuffer, 0, 1, &viewport);
		VkRect2D scissor = {};
		scissor.extent = {extent.width, extent.height};
		vkCmdSetScissor(m_commandBuffer, 0, 1, &scissor);
		return {};
	}

	acm::Error CommandBuffer::bindPipeline(const Pipeline* pipeline)
	{
		if (acm::Error error = requireRecording("bindPipeline"))
			return error;
		// A rejected bind leaves no pipeline bound, so a following draw fails too.
		if (!pipeline || !pipeline->vkPipeline())
		{
			forgetGraphicsPipeline();
			return fail("invalid graphics pipeline");
		}
		if (&owner() != &pipeline->owner())
		{
			forgetGraphicsPipeline();
			return fail("failed to bind graphics pipeline owned by another device");
		}
		if (m_renderingActive)
		{
			const RenderTarget* target = m_renderTarget.backend();
			if (!target || !pipeline->compatibleWith(*target))
			{
				forgetGraphicsPipeline();
				return fail("graphics pipeline is incompatible with active render target");
			}
		}
		vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->vkPipeline());
		rememberGraphicsPipeline(*pipeline);
		return {};
	}

	acm::Error CommandBuffer::bindDescriptorSet(const Pipeline* pipeline, const DescriptorSet* set, const uint32_t* dynamicOffset)
	{
		constexpr const char* call = "bindDescriptorSet";
		if (acm::Error error = requireRecording(call))
			return error;
		if (!pipeline || !pipeline->vkLayout())
			return fail(callError(call, "invalid graphics pipeline"));
		if (!set || !set->vkDescriptorSet())
			return fail(callError(call, "invalid descriptor set"));
		if (&owner() != &pipeline->owner())
			return fail(callError(call, "pipeline is owned by another device"));
		if (&owner() != &set->owner())
			return fail(callError(call, "descriptor set is owned by another device"));
		if (!pipeline->hasDescriptorLayout())
			return fail(callError(call, "the pipeline was created without a descriptor layout"));
		if (!pipeline->acceptsDescriptorSet(*set))
			return fail(callError(call, "the descriptor set's layout does not match the pipeline's descriptor layout"));
		const uint32_t dynamicOffsetCount = dynamicOffset ? 1u : 0u;
		if (set->dynamicDescriptorCount() != dynamicOffsetCount)
			return fail(callError(call, dynamicOffset ? "a dynamic offset needs a set with exactly one UniformBufferDynamic descriptor"
													  : "the set has UniformBufferDynamic descriptors; bind it with a dynamic offset"));
		if (dynamicOffset)
		{
			const size_t offset = *dynamicOffset;
			const size_t alignment = owner().minUniformBufferOffsetAlignment();
			if (alignment > 1 && offset % alignment != 0)
				return fail(acm::Error(std::string(call) + ": dynamic offset " + std::to_string(offset) + " is not a multiple of minUniformBufferOffsetAlignment " + std::to_string(alignment)));
			size_t range = 0;
			size_t bufferSize = 0;
			if (set->dynamicBufferExtent(range, bufferSize) && offset + range > bufferSize)
				return fail(acm::Error(std::string(call) + ": dynamic offset " + std::to_string(offset) + " plus element size " + std::to_string(range) + " exceeds the buffer size " + std::to_string(bufferSize)));
		}
		const VkDescriptorSet descriptorSet = set->vkDescriptorSet();
		vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->vkLayout(), 0, 1, &descriptorSet, dynamicOffsetCount, dynamicOffset);
		return {};
	}

	acm::Error CommandBuffer::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
	{
		if (acm::Error error = requireRecording("draw"))
			return error;
		if (!m_renderTarget.valid() || !m_graphicsPipelineBound)
			return fail("draw requires an active render target and a compatible graphics pipeline");
		vkCmdDraw(m_commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
		return {};
	}

	acm::Error CommandBuffer::bindVertexBuffer(const Buffer* buffer)
	{
		constexpr const char* call = "bindVertexBuffer";
		if (acm::Error error = requireRecording(call))
			return error;
		if (!buffer || !buffer->vkBuffer())
			return fail(callError(call, "invalid vertex buffer"));
		if (&owner() != &buffer->owner())
			return fail(callError(call, "vertex buffer is owned by another device"));
		if (!(buffer->vkUsage() & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT))
			return fail(usageError(call, *buffer, "a vertex buffer; create it with BufferUsage::Vertex"));
		const VkBuffer vkBuffer = buffer->vkBuffer();
		const VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(m_commandBuffer, 0, 1, &vkBuffer, &offset);
		return {};
	}

	acm::Error CommandBuffer::bindIndexBuffer(const Buffer* buffer)
	{
		constexpr const char* call = "bindIndexBuffer";
		if (acm::Error error = requireRecording(call))
			return error;
		if (!buffer || !buffer->vkBuffer())
			return fail(callError(call, "invalid index buffer"));
		if (&owner() != &buffer->owner())
			return fail(callError(call, "index buffer is owned by another device"));
		if (!(buffer->vkUsage() & VK_BUFFER_USAGE_INDEX_BUFFER_BIT))
			return fail(usageError(call, *buffer, "an index buffer; create it with BufferUsage::Index"));
		vkCmdBindIndexBuffer(m_commandBuffer, buffer->vkBuffer(), 0, VK_INDEX_TYPE_UINT32);
		return {};
	}

	acm::Error CommandBuffer::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
	{
		if (acm::Error error = requireRecording("drawIndexed"))
			return error;
		if (!m_renderTarget.valid() || !m_graphicsPipelineBound)
			return fail("indexed draw requires an active render target and a compatible graphics pipeline");
		vkCmdDrawIndexed(m_commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
		return {};
	}

	// -----------------------------------------------------------------------------
	// Transfer
	// -----------------------------------------------------------------------------

	acm::Error CommandBuffer::copyTextureToBuffer(const Texture* texture, const Buffer* buffer)
	{
		constexpr const char* call = "copyTextureToBuffer";
		if (acm::Error error = requireRecording(call))
			return error;
		if (acm::Error error = requireOutsideRendering(call))
			return error;
		if (!texture || !texture->vkImage())
			return fail(callError(call, "invalid texture"));
		if (!buffer || !buffer->vkBuffer())
			return fail(callError(call, "invalid buffer"));
		if (&owner() != &texture->owner())
			return fail(callError(call, "texture is owned by another device"));
		if (&owner() != &buffer->owner())
			return fail(callError(call, "buffer is owned by another device"));
		// Copies go through the color aspect, so depth formats (and Undefined) are out.
		const size_t texelSize = isDepthFormat(texture->format()) ? 0 : bytesPerPixel(texture->format());
		if (texelSize == 0)
			return fail(callError(call, "only color textures can be copied to a buffer"));
		if (!(buffer->vkUsage() & VK_BUFFER_USAGE_TRANSFER_DST_BIT))
			return fail(usageError(call, *buffer, "a transfer destination; create it with BufferUsage::TransferDst"));
		const acm::Extent2D extent = texture->extent();
		const size_t required = size_t(extent.width) * extent.height * texelSize;
		if (buffer->size() < required)
			return fail(acm::Error(std::string(call) + ": buffer is too small (" + std::to_string(buffer->size()) + " bytes, the texture needs " + std::to_string(required) + ")"));
		texture->recordCopyToBuffer(m_commandBuffer, *buffer);
		return {};
	}

	// -----------------------------------------------------------------------------
	// Compute
	// -----------------------------------------------------------------------------

	acm::Error CommandBuffer::bindComputePipeline(const ComputePipeline* pipeline)
	{
		constexpr const char* call = "bindComputePipeline";
		if (acm::Error error = requireRecording(call))
			return error;
		if (!pipeline || !pipeline->vkPipeline())
		{
			m_computePipelineBound = false;
			return fail(callError(call, "invalid compute pipeline"));
		}
		if (&owner() != &pipeline->owner())
		{
			m_computePipelineBound = false;
			return fail(callError(call, "compute pipeline is owned by another device"));
		}
		vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->vkPipeline());
		m_computePipelineBound = true;
		return {};
	}

	acm::Error CommandBuffer::bindComputeDescriptorSet(const ComputePipeline* pipeline, const DescriptorSet* set)
	{
		constexpr const char* call = "bindComputeDescriptorSet";
		if (acm::Error error = requireRecording(call))
			return error;
		if (!pipeline || !pipeline->vkLayout())
			return fail(callError(call, "invalid compute pipeline"));
		if (!set || !set->vkDescriptorSet())
			return fail(callError(call, "invalid descriptor set"));
		if (&owner() != &pipeline->owner())
			return fail(callError(call, "compute pipeline is owned by another device"));
		if (&owner() != &set->owner())
			return fail(callError(call, "descriptor set is owned by another device"));
		if (!pipeline->hasDescriptorLayout())
			return fail(callError(call, "the compute pipeline was created without a descriptor layout"));
		if (!pipeline->acceptsDescriptorSet(*set))
			return fail(callError(call, "the descriptor set's layout does not match the compute pipeline's descriptor layout"));
		if (set->dynamicDescriptorCount() != 0)
			return fail(callError(call, "the set has UniformBufferDynamic descriptors, which a compute bind gives no offset"));
		const VkDescriptorSet descriptorSet = set->vkDescriptorSet();
		vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->vkLayout(), 0, 1, &descriptorSet, 0, nullptr);
		return {};
	}

	acm::Error CommandBuffer::dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
	{
		constexpr const char* call = "dispatch";
		if (acm::Error error = requireRecording(call))
			return error;
		if (acm::Error error = requireOutsideRendering(call))
			return error;
		if (!m_computePipelineBound)
			return fail(callError(call, "requires a bound compute pipeline"));
		const uint32_t* maxGroups = owner().properties().limits.maxComputeWorkGroupCount;
		if (groupsX > maxGroups[0] || groupsY > maxGroups[1] || groupsZ > maxGroups[2])
			return fail(acm::Error(std::string(call) + ": group count (" + std::to_string(groupsX) + ", " + std::to_string(groupsY) + ", " + std::to_string(groupsZ) + ") exceeds the device's maxComputeWorkGroupCount (" + std::to_string(maxGroups[0]) + ", " + std::to_string(maxGroups[1]) + ", " + std::to_string(maxGroups[2]) + ")"));
		vkCmdDispatch(m_commandBuffer, groupsX, groupsY, groupsZ);
		return {};
	}

	// -----------------------------------------------------------------------------
	// Synchronization
	// -----------------------------------------------------------------------------

	acm::Error CommandBuffer::bufferBarrier(const Buffer* buffer, acm::ShaderStage srcStage, acm::ShaderStage dstStage)
	{
		constexpr const char* call = "bufferBarrier";
		if (acm::Error error = requireRecording(call))
			return error;
		// Without dynamicRenderingLocalRead a pipeline barrier is not allowed inside a
		// vkCmdBeginRendering scope.
		if (acm::Error error = requireOutsideRendering(call))
			return error;
		if (!buffer || !buffer->vkBuffer())
			return fail(callError(call, "invalid buffer"));
		if (&owner() != &buffer->owner())
			return fail(callError(call, "buffer is owned by another device"));
		if (uint32_t(srcStage) == 0 || uint32_t(dstStage) == 0)
			return fail(callError(call, "source and destination stages must be non-empty"));
		VkBufferMemoryBarrier2 barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
		barrier.srcStageMask = toVkPipelineStage(srcStage);
		barrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
		barrier.dstStageMask = toVkPipelineStage(dstStage);
		barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.buffer = buffer->vkBuffer();
		barrier.size = VK_WHOLE_SIZE;
		VkDependencyInfo dependency = {};
		dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		dependency.bufferMemoryBarrierCount = 1;
		dependency.pBufferMemoryBarriers = &barrier;
		vkCmdPipelineBarrier2(m_commandBuffer, &dependency);
		return {};
	}

	acm::Error CommandBuffer::transitionImage(const Texture* texture, acm::ImageLayout from, acm::ImageLayout to)
	{
		constexpr const char* call = "transitionImage";
		if (acm::Error error = requireRecording(call))
			return error;
		if (acm::Error error = requireOutsideRendering(call))
			return error;
		if (!texture || !texture->vkImage())
			return fail(callError(call, "invalid texture"));
		if (&owner() != &texture->owner())
			return fail(callError(call, "texture is owned by another device"));
		if (isDepthFormat(texture->format()))
			return fail(callError(call, "depth textures are not supported"));
		if (to == acm::ImageLayout::Undefined)
			return fail(callError(call, "cannot transition to ImageLayout::Undefined"));
		texture->recordTransition(m_commandBuffer, from, to);
		return {};
	}

	// -----------------------------------------------------------------------------
	// Internals
	// -----------------------------------------------------------------------------

	acm::Error CommandBuffer::fail(const char* message)
	{
		return fail(acm::Error(message));
	}

	acm::Error CommandBuffer::fail(acm::Error error)
	{
		if (!m_recordingError)
			m_recordingError = error;
		return error;
	}

	acm::Error CommandBuffer::requireRecording(const char* call)
	{
		if (m_recording)
			return {};
		return fail(callError(call, "command buffer is not recording; call begin() first"));
	}

	acm::Error CommandBuffer::requireOutsideRendering(const char* call)
	{
		if (!m_renderingActive)
			return {};
		return fail(callError(call, "not allowed inside a rendering scope; call endRendering() first"));
	}

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

	void CommandBuffer::resetRecordingState()
	{
		m_recordingError = {};
		m_renderTarget.reset();
		m_renderingActive = false;
		m_computePipelineBound = false;
		forgetGraphicsPipeline();
	}

	void CommandBuffer::release()
	{
		resetRecordingState();
		m_recording = false;
		const VkCommandBuffer commandBuffer = std::exchange(m_commandBuffer, VK_NULL_HANDLE);
		const VkCommandPool commandPool = m_pool.valid() ? m_pool.backend()->vkCommandPool() : VK_NULL_HANDLE;
		Device* owner = std::exchange(m_owner, nullptr);
		if (owner && commandPool && commandBuffer)
			vkFreeCommandBuffers(owner->vkDevice(), commandPool, 1, &commandBuffer);
		m_pool.reset();
	}

} // namespace acm::vulkan
