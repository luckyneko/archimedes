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
		// Graphics recording methods return an error when the active render target and
		// bound pipeline are incompatible; ignored errors are still visible through
		// error() until the next begin() or reset().
		acm::Error beginRendering(const acm::RenderTarget& target, float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 1.0f);
		void endRendering();
		void setViewportAndScissor(acm::Extent2D extent);
		acm::Error bindPipeline(const acm::Pipeline& pipeline);
		// Binds set 0 for a graphics pipeline. The dynamic-offset overload is for a
		// UniformBufferDynamic binding; the offset must satisfy the Device alignment.
		void bindDescriptorSet(const acm::Pipeline& pipeline, const acm::DescriptorSet& set);
		void bindDescriptorSet(const acm::Pipeline& pipeline, const acm::DescriptorSet& set, uint32_t dynamicOffset);
		acm::Error draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0);
		void bindVertexBuffer(const acm::Buffer& buffer);
		void bindIndexBuffer(const acm::Buffer& buffer);
		acm::Error drawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0);

		// Transfer
		// Records a copy from a texture currently in TransferSrc layout to a readback or
		// transfer-destination buffer. For offscreen rendering, create the target with
		// RenderTargetFinish::CopySrc so endRendering leaves the texture ready to copy.
		void copyTextureToBuffer(const acm::Texture& texture, const acm::Buffer& buffer);

		// Compute
		// Compute commands are recorded outside a rendering scope, or in a renderer
		// pre-pass before dynamic rendering begins.
		void bindComputePipeline(const acm::ComputePipeline& pipeline);
		void bindComputeDescriptorSet(const acm::ComputePipeline& pipeline, const acm::DescriptorSet& set);
		void dispatch(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1);

		// Synchronization
		// Makes shader reads/writes of a storage buffer visible between the named stages.
		// Works within one command buffer and across queue-ordered submissions.
		void bufferBarrier(const acm::Buffer& buffer, acm::ShaderStage srcStage, acm::ShaderStage dstStage);
		// Records a texture layout transition for explicit cases such as storage-image
		// compute writes and copy-out/read-after-write paths.
		void transitionImage(const acm::Texture& texture, acm::ImageLayout from, acm::ImageLayout to);

	private:
		// Construction
		friend acm::backend::Device;
		CommandBuffer(acm::ResourceRef<acm::backend::CommandBuffer> resource);
		explicit CommandBuffer(acm::Error error);

		acm::ResourceRef<acm::backend::CommandBuffer> m_resource;
		acm::Error m_error;
	};
} // namespace acm
