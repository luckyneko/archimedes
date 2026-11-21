/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/acmCommandBuffer.h"

#include "archimedes/acmBuffer.h"
#include "archimedes/acmComputePipeline.h"
#include "archimedes/acmDescriptorSet.h"
#include "archimedes/acmPipeline.h"
#include "archimedes/acmRenderTarget.h"
#include "archimedes/acmTexture.h"
#include "archimedes/backendAPI.h"

#include <string>
#include <utility>

namespace acm
{
	// -----------------------------------------------------------------------------
	// Lifetime
	// -----------------------------------------------------------------------------

	CommandBuffer::CommandBuffer() = default;

	CommandBuffer::CommandBuffer(const CommandBuffer& other) = default;

	CommandBuffer& CommandBuffer::operator=(const CommandBuffer& other) = default;

	CommandBuffer::CommandBuffer(CommandBuffer&& other) noexcept = default;

	CommandBuffer& CommandBuffer::operator=(CommandBuffer&& other) noexcept = default;

	CommandBuffer::~CommandBuffer() = default;

	// -----------------------------------------------------------------------------
	// State
	// -----------------------------------------------------------------------------

	void CommandBuffer::reset()
	{
		m_resource.reset();
		m_error = {};
	}

	bool CommandBuffer::valid() const
	{
		return m_resource.valid();
	}

	Error CommandBuffer::error() const
	{
		if (m_error)
			return m_error;
		if (auto* resource = m_resource.access())
			return resource->recordingError();
		return {};
	}

	backend::CommandBuffer* CommandBuffer::backend() const
	{
		return m_resource.access();
	}

	// -----------------------------------------------------------------------------
	// Recording
	// -----------------------------------------------------------------------------

	Error CommandBuffer::begin()
	{
		m_error = {};
		if (auto* resource = m_resource.access())
			return resource->begin();
		return invalid("begin");
	}

	Error CommandBuffer::end()
	{
		if (auto* resource = m_resource.access())
			return resource->end();
		return invalid("end");
	}

	// -----------------------------------------------------------------------------
	// Rendering
	// -----------------------------------------------------------------------------

	Error CommandBuffer::beginRendering(const RenderTarget& target, float r, float g, float b, float a)
	{
		if (auto* resource = m_resource.access())
			return resource->beginRendering(target, r, g, b, a);
		return invalid("beginRendering");
	}

	Error CommandBuffer::endRendering()
	{
		if (auto* resource = m_resource.access())
			return resource->endRendering();
		return invalid("endRendering");
	}

	Error CommandBuffer::setViewportAndScissor(Extent2D extent)
	{
		if (auto* resource = m_resource.access())
			return resource->setViewportAndScissor(extent);
		return invalid("setViewportAndScissor");
	}

	Error CommandBuffer::bindPipeline(const Pipeline& pipeline)
	{
		if (auto* resource = m_resource.access())
			return resource->bindPipeline(pipeline.backend());
		return invalid("bindPipeline");
	}

	Error CommandBuffer::bindDescriptorSet(const Pipeline& pipeline, const DescriptorSet& set)
	{
		if (auto* resource = m_resource.access())
			return resource->bindDescriptorSet(pipeline.backend(), set.backend(), nullptr);
		return invalid("bindDescriptorSet");
	}

	Error CommandBuffer::bindDescriptorSet(const Pipeline& pipeline, const DescriptorSet& set, uint32_t dynamicOffset)
	{
		if (auto* resource = m_resource.access())
			return resource->bindDescriptorSet(pipeline.backend(), set.backend(), &dynamicOffset);
		return invalid("bindDescriptorSet");
	}

	Error CommandBuffer::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
	{
		if (auto* resource = m_resource.access())
			return resource->draw(vertexCount, instanceCount, firstVertex, firstInstance);
		return invalid("draw");
	}

	Error CommandBuffer::bindVertexBuffer(const Buffer& buffer)
	{
		if (auto* resource = m_resource.access())
			return resource->bindVertexBuffer(buffer.backend());
		return invalid("bindVertexBuffer");
	}

	Error CommandBuffer::bindIndexBuffer(const Buffer& buffer)
	{
		if (auto* resource = m_resource.access())
			return resource->bindIndexBuffer(buffer.backend());
		return invalid("bindIndexBuffer");
	}

	Error CommandBuffer::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
	{
		if (auto* resource = m_resource.access())
			return resource->drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
		return invalid("drawIndexed");
	}

	// -----------------------------------------------------------------------------
	// Transfer
	// -----------------------------------------------------------------------------

	Error CommandBuffer::copyTextureToBuffer(const Texture& texture, const Buffer& buffer)
	{
		if (auto* resource = m_resource.access())
			return resource->copyTextureToBuffer(texture.backend(), buffer.backend());
		return invalid("copyTextureToBuffer");
	}

	// -----------------------------------------------------------------------------
	// Compute
	// -----------------------------------------------------------------------------

	Error CommandBuffer::bindComputePipeline(const ComputePipeline& pipeline)
	{
		if (auto* resource = m_resource.access())
			return resource->bindComputePipeline(pipeline.backend());
		return invalid("bindComputePipeline");
	}

	Error CommandBuffer::bindComputeDescriptorSet(const ComputePipeline& pipeline, const DescriptorSet& set)
	{
		if (auto* resource = m_resource.access())
			return resource->bindComputeDescriptorSet(pipeline.backend(), set.backend());
		return invalid("bindComputeDescriptorSet");
	}

	Error CommandBuffer::dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
	{
		if (auto* resource = m_resource.access())
			return resource->dispatch(groupsX, groupsY, groupsZ);
		return invalid("dispatch");
	}

	// -----------------------------------------------------------------------------
	// Synchronization
	// -----------------------------------------------------------------------------

	Error CommandBuffer::bufferBarrier(const Buffer& buffer, ShaderStage srcStage, ShaderStage dstStage)
	{
		if (auto* resource = m_resource.access())
			return resource->bufferBarrier(buffer.backend(), srcStage, dstStage);
		return invalid("bufferBarrier");
	}

	Error CommandBuffer::transitionImage(const Texture& texture, ImageLayout from, ImageLayout to)
	{
		if (auto* resource = m_resource.access())
			return resource->transitionImage(texture.backend(), from, to);
		return invalid("transitionImage");
	}

	// -----------------------------------------------------------------------------
	// Construction
	// -----------------------------------------------------------------------------

	CommandBuffer::CommandBuffer(ResourceRef<backend::CommandBuffer> resource)
		: m_resource(std::move(resource))
	{
	}

	CommandBuffer::CommandBuffer(Error error)
		: m_error(std::move(error))
	{
	}

	// -----------------------------------------------------------------------------
	// Internals
	// -----------------------------------------------------------------------------

	Error CommandBuffer::invalid(const char* call)
	{
		Error error(std::string(call) + ": invalid command buffer");
		if (!m_error)
			m_error = error;
		return error;
	}

} // namespace acm
