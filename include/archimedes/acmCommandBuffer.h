#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmTypes.h"
#include "archimedes/acmVkFwd.h"
#include <cstdint>

namespace acm
{
	// A recording handle over a pool-allocated VkCommandBuffer. The buffer is owned
	// by its CommandPool (freed when the pool is destroyed); this handle keeps the
	// pool alive and exposes the subset of recording commands the renderer needs.
	class CommandBuffer
	{
	public:
		CommandBuffer() {}

		inline void reset() { m.reset(); m_error = {}; }
		inline bool valid() const { return m != nullptr; }
		acm::Error error() const { return m_error; }

		acm::Error begin();
		acm::Error end();
		void beginRenderPass(acm::RenderTarget target, float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 1.0f);
		void endRenderPass();
		// Sets the dynamic viewport + scissor to cover the whole extent. Required
		// before drawing with a pipeline that uses dynamic viewport/scissor state.
		void setViewportAndScissor(acm::Extent2D extent);
		void bindPipeline(acm::Pipeline pipeline);
		// Binds a descriptor set (set 0) for the given pipeline's layout. Call after
		// bindPipeline and before draw.
		void bindDescriptorSet(acm::Pipeline pipeline, acm::DescriptorSet set);
		// As above, but supplies the per-draw byte offset for a set that has a single
		// dynamic uniform binding (DescriptorType::UniformBufferDynamic). `dynamicOffset`
		// must be a multiple of Device::minUniformBufferOffsetAlignment().
		void bindDescriptorSet(acm::Pipeline pipeline, acm::DescriptorSet set, uint32_t dynamicOffset);
		void draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0);
		// Binds a vertex buffer at binding 0 / an index buffer (32-bit indices). Bind
		// before drawIndexed; the pipeline's vertex layout must match the buffer.
		void bindVertexBuffer(acm::Buffer buffer);
		void bindIndexBuffer(acm::Buffer buffer);
		void drawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0);
		// Copies the whole texture into buffer (tightly packed). The texture must be
		// in TRANSFER_SRC layout — which an offscreen RenderTarget leaves it in.
		void copyTextureToBuffer(acm::Texture texture, acm::Buffer buffer);

		// Compute. The same record-then-submit flow as graphics, but on the COMPUTE bind
		// point and outside any render pass: bindComputePipeline -> bindComputeDescriptorSet
		// -> dispatch the given number of workgroups (each runs the shader's local_size).
		void bindComputePipeline(acm::ComputePipeline pipeline);
		void bindComputeDescriptorSet(acm::ComputePipeline pipeline, acm::DescriptorSet set);
		void dispatch(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1);
		// Inserts a storage-buffer memory dependency so accesses in `srcStage` complete
		// (and become visible) before accesses in `dstStage` — e.g. a compute dispatch
		// that wrote `buffer` (srcStage Compute) feeding a draw that reads it (dstStage
		// Vertex/Fragment), or the reverse before overwriting it. Uses a conservative
		// shader-read|write access mask on both sides, so it covers read-after-write,
		// write-after-read, and write-after-write on the buffer. The dependency holds
		// within one command buffer and across submissions in queue submission order.
		void bufferBarrier(acm::Buffer buffer, acm::ShaderStage srcStage, acm::ShaderStage dstStage);
		// Transitions a texture's layout (an image memory barrier), inferring conservative
		// access + pipeline stages from the layouts. Use around a storage-image compute
		// write: `Undefined → General` to write it, then `General → TransferSrc` (to copy
		// out) or `General → ShaderReadOnly` (to sample). Covers all mip levels.
		void transitionImage(acm::Texture texture, acm::ImageLayout from, acm::ImageLayout to);

		VkCommandBuffer vkCommandBuffer() const;

	private:
		friend class CommandPool; // only CommandPool::allocate builds one
		CommandBuffer(acm::CommandPool pool, VkCommandBuffer commandBuffer);

		struct impl;
		std::shared_ptr<impl> m;
		acm::Error m_error;
	};
} // namespace acm
