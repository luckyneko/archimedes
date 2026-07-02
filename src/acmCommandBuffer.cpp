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

acm::CommandBuffer::CommandBuffer() = default;

acm::CommandBuffer::CommandBuffer(acm::ResourceRef<acm::native::CommandBuffer> resource)
	: m_resource(std::move(resource))
{
}

acm::CommandBuffer::CommandBuffer(acm::Error error)
	: m_error(std::move(error))
{
}

acm::CommandBuffer::CommandBuffer(const acm::CommandBuffer& other) = default;

acm::CommandBuffer& acm::CommandBuffer::operator=(const acm::CommandBuffer& other) = default;

acm::CommandBuffer::CommandBuffer(acm::CommandBuffer&& other) noexcept = default;

acm::CommandBuffer& acm::CommandBuffer::operator=(acm::CommandBuffer&& other) noexcept = default;

acm::CommandBuffer::~CommandBuffer() = default;

void acm::CommandBuffer::reset()
{
	m_resource.reset();
	m_error = {};
}

bool acm::CommandBuffer::valid() const
{
	return m_resource.valid();
}

acm::Error acm::CommandBuffer::error() const
{
	return m_error;
}

acm::native::CommandBuffer* acm::CommandBuffer::native() const
{
	return m_resource.access();
}

acm::Error acm::CommandBuffer::begin()
{
	if (auto* resource = m_resource.access())
		return resource->begin();
	return acm::Error("invalid command buffer");
}

acm::Error acm::CommandBuffer::end()
{
	if (auto* resource = m_resource.access())
		return resource->end();
	return acm::Error("invalid command buffer");
}

void acm::CommandBuffer::beginRendering(const acm::RenderTarget& target, float r, float g, float b, float a)
{
	if (auto* resource = m_resource.access())
		resource->beginRendering(target, r, g, b, a);
}

void acm::CommandBuffer::endRendering()
{
	if (auto* resource = m_resource.access())
		resource->endRendering();
}

void acm::CommandBuffer::setViewportAndScissor(acm::Extent2D extent)
{
	if (auto* resource = m_resource.access())
		resource->setViewportAndScissor(extent);
}

void acm::CommandBuffer::bindPipeline(const acm::Pipeline& pipeline)
{
	if (auto* resource = m_resource.access())
		if (pipeline.native())
			resource->bindPipeline(*pipeline.native());
}

void acm::CommandBuffer::bindDescriptorSet(const acm::Pipeline& pipeline, const acm::DescriptorSet& set)
{
	if (auto* resource = m_resource.access())
		if (pipeline.native() && set.native())
			resource->bindDescriptorSet(*pipeline.native(), *set.native(), nullptr);
}

void acm::CommandBuffer::bindDescriptorSet(const acm::Pipeline& pipeline, const acm::DescriptorSet& set, uint32_t dynamicOffset)
{
	if (auto* resource = m_resource.access())
		if (pipeline.native() && set.native())
			resource->bindDescriptorSet(*pipeline.native(), *set.native(), &dynamicOffset);
}

void acm::CommandBuffer::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
	if (auto* resource = m_resource.access())
		resource->draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void acm::CommandBuffer::bindVertexBuffer(const acm::Buffer& buffer)
{
	if (auto* resource = m_resource.access())
		if (buffer.native())
			resource->bindVertexBuffer(*buffer.native());
}

void acm::CommandBuffer::bindIndexBuffer(const acm::Buffer& buffer)
{
	if (auto* resource = m_resource.access())
		if (buffer.native())
			resource->bindIndexBuffer(*buffer.native());
}

void acm::CommandBuffer::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
	if (auto* resource = m_resource.access())
		resource->drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void acm::CommandBuffer::copyTextureToBuffer(const acm::Texture& texture, const acm::Buffer& buffer)
{
	if (auto* resource = m_resource.access())
		if (texture.native() && buffer.native())
			resource->copyTextureToBuffer(*texture.native(), *buffer.native());
}

void acm::CommandBuffer::bindComputePipeline(const acm::ComputePipeline& pipeline)
{
	if (auto* resource = m_resource.access())
		if (pipeline.native())
			resource->bindComputePipeline(*pipeline.native());
}

void acm::CommandBuffer::bindComputeDescriptorSet(const acm::ComputePipeline& pipeline, const acm::DescriptorSet& set)
{
	if (auto* resource = m_resource.access())
		if (pipeline.native() && set.native())
			resource->bindComputeDescriptorSet(*pipeline.native(), *set.native());
}

void acm::CommandBuffer::dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
{
	if (auto* resource = m_resource.access())
		resource->dispatch(groupsX, groupsY, groupsZ);
}

void acm::CommandBuffer::bufferBarrier(const acm::Buffer& buffer, acm::ShaderStage srcStage, acm::ShaderStage dstStage)
{
	if (auto* resource = m_resource.access())
		if (buffer.native())
			resource->bufferBarrier(*buffer.native(), srcStage, dstStage);
}

void acm::CommandBuffer::transitionImage(const acm::Texture& texture, acm::ImageLayout from, acm::ImageLayout to)
{
	if (auto* resource = m_resource.access())
		if (texture.native())
			resource->transitionImage(*texture.native(), from, to);
}
