/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmBackend.h"
#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmResourceRef.h"
#include "archimedes/acmTypes.h"

#include <cstdint>

namespace acm
{
	// Copyable handle to one primary command buffer allocated from a CommandPool.
	// Commands are recorded between begin() and end(); the owning Device/CommandPool
	// must outlive the handle and all copied handles. Renderer::render supplies an
	// already-begun buffer to its callbacks.
	//
	// Errors. Every recording call returns an acm::Error. A call that rejects its input
	// (an invalid or stale argument, one owned by another Device, a buffer that is not
	// recording, a command on the wrong side of a rendering scope, or missing state the
	// command needs) records nothing and returns a specific error. The first such error
	// since begin() is also kept as the sticky error(): first error wins, so later failures
	// still return their own error but do not replace it, and a run of unchecked calls can
	// be checked once at the end. begin() clears it; reset() drops it with the handle. It
	// belongs to the command buffer, not the handle, so every copy reports the same one.
	// Renderer::render and Device::submitSync(record) check it and do not submit a failed
	// recording; Device::submitSync(commandBuffer) submits what was recorded, so check
	// error() first when partial work must not run.
	//
	// Lifetime. A command buffer does not retain the resources recorded into it (buffers,
	// textures, pipelines, descriptor sets, render targets). Destroying a dropped resource
	// is deferred only past submissions already made, so a resource whose last wrapper is
	// dropped after it was recorded but before that recording is submitted can be
	// destroyed (by any garbage collection, on any thread) before the GPU uses it. Keep
	// every recorded resource alive until Renderer::render() or Device::submitSync()
	// returns. What is written into a DescriptorSet is the exception: the set retains it,
	// so keeping the set alive is enough.
	class CommandBuffer
	{
	public:
		// Lifetime
		CommandBuffer();
		CommandBuffer(const acm::CommandBuffer& other);
		CommandBuffer& operator=(const acm::CommandBuffer& other);
		CommandBuffer(acm::CommandBuffer&& other) noexcept;
		CommandBuffer& operator=(acm::CommandBuffer&& other) noexcept;
		~CommandBuffer();

		// State
		void reset();
		bool valid() const;
		acm::Error error() const;
		acm::backend::CommandBuffer* backend() const;

		// Recording
		acm::Error begin();
		acm::Error end();

		// Rendering
		// Begins dynamic rendering over target and clears color/depth attachments. Only
		// one rendering scope may be open at a time; Renderer::render opens the swapchain
		// target for the draw callback, while pre-pass callbacks run outside that scope.
		// bindPipeline and the draws fail when the active render target and the bound
		// pipeline are incompatible. end() closes a scope left open, and reports it.
		acm::Error beginRendering(const acm::RenderTarget& target, float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 1.0f);
		acm::Error endRendering();
		// The extent must be non-zero.
		acm::Error setViewportAndScissor(acm::Extent2D extent);
		acm::Error bindPipeline(const acm::Pipeline& pipeline);
		// Binds set 0 for a graphics pipeline built with a descriptor layout matching the
		// set's. The dynamic-offset overload is for a set with exactly one
		// UniformBufferDynamic descriptor, the plain overload for a set with none. The offset
		// must be a multiple of Device::minUniformBufferOffsetAlignment and, with the element
		// size given to setDynamicBuffer, stay inside the buffer.
		acm::Error bindDescriptorSet(const acm::Pipeline& pipeline, const acm::DescriptorSet& set);
		acm::Error bindDescriptorSet(const acm::Pipeline& pipeline, const acm::DescriptorSet& set, uint32_t dynamicOffset);
		acm::Error draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0);
		// The buffer must have been created with BufferUsage::Vertex / BufferUsage::Index.
		acm::Error bindVertexBuffer(const acm::Buffer& buffer);
		acm::Error bindIndexBuffer(const acm::Buffer& buffer);
		acm::Error drawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0);

		// Transfer
		// Records, outside a rendering scope, a copy from a color texture currently in
		// TransferSrc layout to a buffer that can be a transfer destination
		// (BufferUsage::TransferDst, or the staged Vertex / Index) of at least
		// width x height x texel-size bytes. For offscreen rendering, create the target with
		// RenderTargetFinish::CopySrc so endRendering leaves the texture ready to copy.
		acm::Error copyTextureToBuffer(const acm::Texture& texture, const acm::Buffer& buffer);

		// Compute
		// Compute commands are recorded outside a rendering scope, or in a renderer
		// pre-pass before dynamic rendering begins. dispatch needs a bound compute
		// pipeline. bindComputeDescriptorSet needs a pipeline built with a descriptor layout
		// and a set allocated from that layout or one with identical bindings; a compute
		// descriptor set takes no dynamic offset.
		acm::Error bindComputePipeline(const acm::ComputePipeline& pipeline);
		acm::Error bindComputeDescriptorSet(const acm::ComputePipeline& pipeline, const acm::DescriptorSet& set);
		acm::Error dispatch(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1);

		// Synchronization
		// Barriers are recorded outside a rendering scope.
		// Makes shader reads/writes of a storage buffer visible between the named (non-empty)
		// stages. Works within one command buffer and across queue-ordered submissions.
		acm::Error bufferBarrier(const acm::Buffer& buffer, acm::ShaderStage srcStage, acm::ShaderStage dstStage);
		// Records a color texture layout transition for explicit cases such as storage-image
		// compute writes and copy-out/read-after-write paths. `to` may not be Undefined.
		acm::Error transitionImage(const acm::Texture& texture, acm::ImageLayout from, acm::ImageLayout to);

	private:
		// Construction
		friend acm::backend::Device;
		CommandBuffer(acm::ResourceRef<acm::backend::CommandBuffer> resource);
		explicit CommandBuffer(acm::Error error);

		// Internals
		// The error for a call on a handle with no live command buffer; kept in m_error.
		acm::Error invalid(const char* call);

		acm::ResourceRef<acm::backend::CommandBuffer> m_resource;
		// Only for a handle without a live command buffer (construction failure, or calls on
		// an invalid handle). Recording errors live in the backend command buffer.
		acm::Error m_error;
	};
} // namespace acm
