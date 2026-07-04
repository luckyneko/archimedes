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
			return resource->error();
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
		{
			if (acm::Error error = resource->begin())
			{
				m_error = error;
				return error;
			}
			return {};
		}
		m_error = Error("invalid command buffer");
		return m_error;
	}

	Error CommandBuffer::end()
	{
		if (auto* resource = m_resource.access())
		{
			if (acm::Error error = resource->end())
			{
				m_error = error;
				return error;
			}
			return {};
		}
		m_error = Error("invalid command buffer");
		return m_error;
	}

	// -----------------------------------------------------------------------------
	// Rendering
	// -----------------------------------------------------------------------------

	Error CommandBuffer::beginRendering(const RenderTarget& target, float r, float g, float b, float a)
	{
		if (auto* resource = m_resource.access())
		{
			if (acm::Error error = resource->beginRendering(target, r, g, b, a))
			{
				m_error = error;
				return error;
			}
			return {};
		}
		m_error = Error("invalid command buffer");
		return m_error;
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

	Error CommandBuffer::bindPipeline(const Pipeline& pipeline)
	{
		if (auto* resource = m_resource.access())
		{
			if (pipeline.backend())
			{
				if (acm::Error error = resource->bindPipeline(*pipeline.backend()))
				{
					m_error = error;
					return error;
				}
				return {};
			}
			m_error = Error("invalid graphics pipeline");
			return m_error;
		}
		m_error = Error("invalid command buffer");
		return m_error;
	}

	void CommandBuffer::bindDescriptorSet(const Pipeline& pipeline, const DescriptorSet& set)
	{
		if (auto* resource = m_resource.access())
		{
			if (pipeline.backend() && set.backend())
				resource->bindDescriptorSet(*pipeline.backend(), *set.backend(), nullptr);
		}
	}

	void CommandBuffer::bindDescriptorSet(const Pipeline& pipeline, const DescriptorSet& set, uint32_t dynamicOffset)
	{
		if (auto* resource = m_resource.access())
		{
			if (pipeline.backend() && set.backend())
				resource->bindDescriptorSet(*pipeline.backend(), *set.backend(), &dynamicOffset);
		}
	}

	Error CommandBuffer::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
	{
		if (auto* resource = m_resource.access())
		{
			if (acm::Error error = resource->draw(vertexCount, instanceCount, firstVertex, firstInstance))
			{
				m_error = error;
				return error;
			}
			return {};
		}
		m_error = Error("invalid command buffer");
		return m_error;
	}

	void CommandBuffer::bindVertexBuffer(const Buffer& buffer)
	{
		if (auto* resource = m_resource.access())
		{
			if (buffer.backend())
				resource->bindVertexBuffer(*buffer.backend());
		}
	}

	void CommandBuffer::bindIndexBuffer(const Buffer& buffer)
	{
		if (auto* resource = m_resource.access())
		{
			if (buffer.backend())
				resource->bindIndexBuffer(*buffer.backend());
		}
	}

	Error CommandBuffer::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
	{
		if (auto* resource = m_resource.access())
		{
			if (acm::Error error = resource->drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance))
			{
				m_error = error;
				return error;
			}
			return {};
		}
		m_error = Error("invalid command buffer");
		return m_error;
	}

	// -----------------------------------------------------------------------------
	// Transfer
	// -----------------------------------------------------------------------------

	void CommandBuffer::copyTextureToBuffer(const Texture& texture, const Buffer& buffer)
	{
		if (auto* resource = m_resource.access())
		{
			if (texture.backend() && buffer.backend())
				resource->copyTextureToBuffer(*texture.backend(), *buffer.backend());
		}
	}

	// -----------------------------------------------------------------------------
	// Compute
	// -----------------------------------------------------------------------------

	void CommandBuffer::bindComputePipeline(const ComputePipeline& pipeline)
	{
		if (auto* resource = m_resource.access())
		{
			if (pipeline.backend())
				resource->bindComputePipeline(*pipeline.backend());
		}
	}

	void CommandBuffer::bindComputeDescriptorSet(const ComputePipeline& pipeline, const DescriptorSet& set)
	{
		if (auto* resource = m_resource.access())
		{
			if (pipeline.backend() && set.backend())
				resource->bindComputeDescriptorSet(*pipeline.backend(), *set.backend());
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
			if (buffer.backend())
				resource->bufferBarrier(*buffer.backend(), srcStage, dstStage);
		}
	}

	void CommandBuffer::transitionImage(const Texture& texture, ImageLayout from, ImageLayout to)
	{
		if (auto* resource = m_resource.access())
		{
			if (texture.backend())
				resource->transitionImage(*texture.backend(), from, to);
		}
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

} // namespace acm
