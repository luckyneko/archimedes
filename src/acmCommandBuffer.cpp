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
#include "archimedes/nativeAPI.h"

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
		return m_error;
	}

	native::CommandBuffer* CommandBuffer::native() const
	{
		return m_resource.access();
	}

	// -----------------------------------------------------------------------------
	// Recording
	// -----------------------------------------------------------------------------

	Error CommandBuffer::begin()
	{
		if (auto* resource = m_resource.access())
			return resource->begin();
		return Error("invalid command buffer");
	}

	Error CommandBuffer::end()
	{
		if (auto* resource = m_resource.access())
			return resource->end();
		return Error("invalid command buffer");
	}

	// -----------------------------------------------------------------------------
	// Rendering
	// -----------------------------------------------------------------------------

	void CommandBuffer::beginRendering(const RenderTarget& target, float r, float g, float b, float a)
	{
		if (auto* resource = m_resource.access())
			resource->beginRendering(target, r, g, b, a);
	}

	void CommandBuffer::endRendering()
	{
		if (auto* resource = m_resource.access())
			resource->endRendering();
	}

	void CommandBuffer::setViewportAndScissor(Extent2D extent)
	{
		if (auto* resource = m_resource.access())
			resource->setViewportAndScissor(extent);
	}

	void CommandBuffer::bindPipeline(const Pipeline& pipeline)
	{
		if (auto* resource = m_resource.access())
		{
			if (pipeline.native())
				resource->bindPipeline(*pipeline.native());
		}
	}

	void CommandBuffer::bindDescriptorSet(const Pipeline& pipeline, const DescriptorSet& set)
	{
		if (auto* resource = m_resource.access())
		{
			if (pipeline.native() && set.native())
				resource->bindDescriptorSet(*pipeline.native(), *set.native(), nullptr);
		}
	}

	void CommandBuffer::bindDescriptorSet(const Pipeline& pipeline, const DescriptorSet& set, uint32_t dynamicOffset)
	{
		if (auto* resource = m_resource.access())
		{
			if (pipeline.native() && set.native())
				resource->bindDescriptorSet(*pipeline.native(), *set.native(), &dynamicOffset);
		}
	}

	void CommandBuffer::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
	{
		if (auto* resource = m_resource.access())
			resource->draw(vertexCount, instanceCount, firstVertex, firstInstance);
	}

	void CommandBuffer::bindVertexBuffer(const Buffer& buffer)
	{
		if (auto* resource = m_resource.access())
		{
			if (buffer.native())
				resource->bindVertexBuffer(*buffer.native());
		}
	}

	void CommandBuffer::bindIndexBuffer(const Buffer& buffer)
	{
		if (auto* resource = m_resource.access())
		{
			if (buffer.native())
				resource->bindIndexBuffer(*buffer.native());
		}
	}

	void CommandBuffer::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
	{
		if (auto* resource = m_resource.access())
			resource->drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
	}

	// -----------------------------------------------------------------------------
	// Transfer
	// -----------------------------------------------------------------------------

	void CommandBuffer::copyTextureToBuffer(const Texture& texture, const Buffer& buffer)
	{
		if (auto* resource = m_resource.access())
		{
			if (texture.native() && buffer.native())
				resource->copyTextureToBuffer(*texture.native(), *buffer.native());
		}
	}

	// -----------------------------------------------------------------------------
	// Compute
	// -----------------------------------------------------------------------------

	void CommandBuffer::bindComputePipeline(const ComputePipeline& pipeline)
	{
		if (auto* resource = m_resource.access())
		{
			if (pipeline.native())
				resource->bindComputePipeline(*pipeline.native());
		}
	}

	void CommandBuffer::bindComputeDescriptorSet(const ComputePipeline& pipeline, const DescriptorSet& set)
	{
		if (auto* resource = m_resource.access())
		{
			if (pipeline.native() && set.native())
				resource->bindComputeDescriptorSet(*pipeline.native(), *set.native());
		}
	}

	void CommandBuffer::dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
	{
		if (auto* resource = m_resource.access())
			resource->dispatch(groupsX, groupsY, groupsZ);
	}

	// -----------------------------------------------------------------------------
	// Synchronization
	// -----------------------------------------------------------------------------

	void CommandBuffer::bufferBarrier(const Buffer& buffer, ShaderStage srcStage, ShaderStage dstStage)
	{
		if (auto* resource = m_resource.access())
		{
			if (buffer.native())
				resource->bufferBarrier(*buffer.native(), srcStage, dstStage);
		}
	}

	void CommandBuffer::transitionImage(const Texture& texture, ImageLayout from, ImageLayout to)
	{
		if (auto* resource = m_resource.access())
		{
			if (texture.native())
				resource->transitionImage(*texture.native(), from, to);
		}
	}

	// -----------------------------------------------------------------------------
	// Construction
	// -----------------------------------------------------------------------------

	CommandBuffer::CommandBuffer(ResourceRef<native::CommandBuffer> resource)
		: m_resource(std::move(resource))
	{
	}

	CommandBuffer::CommandBuffer(Error error)
		: m_error(std::move(error))
	{
	}

} // namespace acm
