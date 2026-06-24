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

acm::CommandBuffer::CommandBuffer(acm::native::CommandBuffer* resource, acm::Handle handle)
	: m_resource(resource)
	, m_handle(handle)
{
}

acm::CommandBuffer::CommandBuffer(acm::Error error)
	: m_error(std::move(error))
{
}

acm::CommandBuffer::CommandBuffer(const acm::CommandBuffer& other)
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(other.m_error)
{
	if (m_resource && !m_resource->retain(m_handle))
	{
		m_resource = nullptr;
		m_handle.reset();
	}
}

acm::CommandBuffer& acm::CommandBuffer::operator=(const acm::CommandBuffer& other)
{
	if (this == &other)
		return *this;
	reset();
	m_resource = other.m_resource;
	m_handle = other.m_handle;
	m_error = other.m_error;
	if (m_resource && !m_resource->retain(m_handle))
	{
		m_resource = nullptr;
		m_handle.reset();
	}
	return *this;
}

acm::CommandBuffer::CommandBuffer(acm::CommandBuffer&& other) noexcept
	: m_resource(other.m_resource)
	, m_handle(other.m_handle)
	, m_error(std::move(other.m_error))
{
	other.m_resource = nullptr;
	other.m_handle.reset();
}

acm::CommandBuffer& acm::CommandBuffer::operator=(acm::CommandBuffer&& other) noexcept
{
	if (this == &other)
		return *this;
	reset();
	m_resource = other.m_resource;
	m_handle = other.m_handle;
	m_error = std::move(other.m_error);
	other.m_resource = nullptr;
	other.m_handle.reset();
	return *this;
}

acm::CommandBuffer::~CommandBuffer()
{
	reset();
}

void acm::CommandBuffer::reset()
{
	if (m_resource)
		m_resource->release(m_handle);
	m_resource = nullptr;
	m_handle.reset();
	m_error = {};
}

bool acm::CommandBuffer::valid() const
{
	return m_resource && m_resource->valid(m_handle);
}

acm::Error acm::CommandBuffer::error() const
{
	return m_error;
}

acm::Error acm::CommandBuffer::begin()
{
	return m_resource ? m_resource->begin(m_handle) : acm::Error("invalid command buffer");
}

acm::Error acm::CommandBuffer::end()
{
	return m_resource ? m_resource->end(m_handle) : acm::Error("invalid command buffer");
}

void acm::CommandBuffer::beginRenderPass(const acm::RenderTarget& target, float r, float g, float b, float a)
{
	if (m_resource && target.native())
		m_resource->beginRenderPass(m_handle, *target.native(), target.handle(), r, g, b, a);
}

void acm::CommandBuffer::endRenderPass()
{
	if (m_resource)
		m_resource->endRenderPass(m_handle);
}

void acm::CommandBuffer::setViewportAndScissor(acm::Extent2D extent)
{
	if (m_resource)
		m_resource->setViewportAndScissor(m_handle, extent);
}

void acm::CommandBuffer::bindPipeline(const acm::Pipeline& pipeline)
{
	if (m_resource && pipeline.native())
		m_resource->bindPipeline(m_handle, *pipeline.native(), pipeline.handle());
}

void acm::CommandBuffer::bindDescriptorSet(const acm::Pipeline& pipeline, const acm::DescriptorSet& set)
{
	if (m_resource && pipeline.native() && set.native())
		m_resource->bindDescriptorSet(m_handle, *pipeline.native(), pipeline.handle(), *set.native(), set.handle(), nullptr);
}

void acm::CommandBuffer::bindDescriptorSet(const acm::Pipeline& pipeline, const acm::DescriptorSet& set, uint32_t dynamicOffset)
{
	if (m_resource && pipeline.native() && set.native())
		m_resource->bindDescriptorSet(m_handle, *pipeline.native(), pipeline.handle(), *set.native(), set.handle(), &dynamicOffset);
}

void acm::CommandBuffer::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
	if (m_resource)
		m_resource->draw(m_handle, vertexCount, instanceCount, firstVertex, firstInstance);
}

void acm::CommandBuffer::bindVertexBuffer(const acm::Buffer& buffer)
{
	if (m_resource && buffer.native())
		m_resource->bindVertexBuffer(m_handle, *buffer.native(), buffer.handle());
}

void acm::CommandBuffer::bindIndexBuffer(const acm::Buffer& buffer)
{
	if (m_resource && buffer.native())
		m_resource->bindIndexBuffer(m_handle, *buffer.native(), buffer.handle());
}

void acm::CommandBuffer::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
	if (m_resource)
		m_resource->drawIndexed(m_handle, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void acm::CommandBuffer::copyTextureToBuffer(const acm::Texture& texture, const acm::Buffer& buffer)
{
	if (m_resource && texture.native() && buffer.native())
		m_resource->copyTextureToBuffer(m_handle, *texture.native(), texture.handle(), *buffer.native(), buffer.handle());
}

void acm::CommandBuffer::bindComputePipeline(const acm::ComputePipeline& pipeline)
{
	if (m_resource && pipeline.native())
		m_resource->bindComputePipeline(m_handle, *pipeline.native(), pipeline.handle());
}

void acm::CommandBuffer::bindComputeDescriptorSet(const acm::ComputePipeline& pipeline, const acm::DescriptorSet& set)
{
	if (m_resource && pipeline.native() && set.native())
		m_resource->bindComputeDescriptorSet(m_handle, *pipeline.native(), pipeline.handle(), *set.native(), set.handle());
}

void acm::CommandBuffer::dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
{
	if (m_resource)
		m_resource->dispatch(m_handle, groupsX, groupsY, groupsZ);
}

void acm::CommandBuffer::bufferBarrier(const acm::Buffer& buffer, acm::ShaderStage srcStage, acm::ShaderStage dstStage)
{
	if (m_resource && buffer.native())
		m_resource->bufferBarrier(m_handle, *buffer.native(), buffer.handle(), srcStage, dstStage);
}

void acm::CommandBuffer::transitionImage(const acm::Texture& texture, acm::ImageLayout from, acm::ImageLayout to)
{
	if (m_resource && texture.native())
		m_resource->transitionImage(m_handle, *texture.native(), texture.handle(), from, to);
}
