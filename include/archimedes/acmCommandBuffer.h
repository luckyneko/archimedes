#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmHandle.h"
#include "archimedes/acmNative.h"
#include "archimedes/acmTypes.h"

#include <cstdint>

namespace acm
{
	class CommandBuffer
	{
	public:
		CommandBuffer();
		CommandBuffer(const acm::CommandBuffer& other);
		CommandBuffer& operator=(const acm::CommandBuffer& other);
		CommandBuffer(acm::CommandBuffer&& other) noexcept;
		CommandBuffer& operator=(acm::CommandBuffer&& other) noexcept;
		~CommandBuffer();

		void reset();
		bool valid() const;
		acm::Error error() const;
		const acm::Handle& handle() const { return m_handle; }
		acm::native::CommandBuffer* native() const { return m_resource; }

		acm::Error begin();
		acm::Error end();

		void beginRenderPass(const acm::RenderTarget& target, float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 1.0f);
		void endRenderPass();
		void setViewportAndScissor(acm::Extent2D extent);
		void bindPipeline(const acm::Pipeline& pipeline);
		void bindDescriptorSet(const acm::Pipeline& pipeline, const acm::DescriptorSet& set);
		void bindDescriptorSet(const acm::Pipeline& pipeline, const acm::DescriptorSet& set, uint32_t dynamicOffset);
		void draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0);
		void bindVertexBuffer(const acm::Buffer& buffer);
		void bindIndexBuffer(const acm::Buffer& buffer);
		void drawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0);
		void copyTextureToBuffer(const acm::Texture& texture, const acm::Buffer& buffer);
		void bindComputePipeline(const acm::ComputePipeline& pipeline);
		void bindComputeDescriptorSet(const acm::ComputePipeline& pipeline, const acm::DescriptorSet& set);
		void dispatch(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1);
		void bufferBarrier(const acm::Buffer& buffer, acm::ShaderStage srcStage, acm::ShaderStage dstStage);
		void transitionImage(const acm::Texture& texture, acm::ImageLayout from, acm::ImageLayout to);

	private:
		friend acm::native::Device;
		CommandBuffer(acm::native::CommandBuffer* resource, acm::Handle handle);
		explicit CommandBuffer(acm::Error error);

		acm::native::CommandBuffer* m_resource{nullptr};
		acm::Handle m_handle;
		acm::Error m_error;
	};
} // namespace acm
