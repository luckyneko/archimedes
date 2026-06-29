#pragma once

#include "archimedes/acmCommandPool.h"
#include "archimedes/acmError.h"
#include "archimedes/acmRenderTarget.h"
#include "archimedes/acmTypes.h"
#include "archimedes/HandleMap.h"

#include <vulkan/vulkan.h>

namespace acm::vulkan
{
	class Device;
	class Buffer;
	class CommandPool;
	class ComputePipeline;
	class DescriptorSet;
	class Pipeline;
	class RenderTarget;
	class Texture;

	class CommandBuffer : public acm::ResourceSlot<acm::vulkan::CommandBuffer, acm::vulkan::Device>
	{
	public:
		bool create(acm::vulkan::Device& owner, const acm::CommandPool& pool);
		VkCommandBuffer vkCommandBuffer(const acm::Handle& handle) const;

		acm::Error begin(const acm::Handle& handle);
		acm::Error end(const acm::Handle& handle);
		void beginRendering(const acm::Handle& handle, const acm::RenderTarget& target, float r, float g, float b, float a);
		void endRendering(const acm::Handle& handle);
		void setViewportAndScissor(const acm::Handle& handle, acm::Extent2D extent);
		void bindPipeline(const acm::Handle& handle, const acm::vulkan::Pipeline& pipeline, const acm::Handle& pipelineHandle);
		void bindDescriptorSet(const acm::Handle& handle, const acm::vulkan::Pipeline& pipeline, const acm::Handle& pipelineHandle, const acm::vulkan::DescriptorSet& set, const acm::Handle& setHandle, const uint32_t* dynamicOffset);
		void draw(const acm::Handle& handle, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
		void bindVertexBuffer(const acm::Handle& handle, const acm::vulkan::Buffer& buffer, const acm::Handle& bufferHandle);
		void bindIndexBuffer(const acm::Handle& handle, const acm::vulkan::Buffer& buffer, const acm::Handle& bufferHandle);
		void drawIndexed(const acm::Handle& handle, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);
		void copyTextureToBuffer(const acm::Handle& handle, const acm::vulkan::Texture& texture, const acm::Handle& textureHandle, const acm::vulkan::Buffer& buffer, const acm::Handle& bufferHandle);
		void bindComputePipeline(const acm::Handle& handle, const acm::vulkan::ComputePipeline& pipeline, const acm::Handle& pipelineHandle);
		void bindComputeDescriptorSet(const acm::Handle& handle, const acm::vulkan::ComputePipeline& pipeline, const acm::Handle& pipelineHandle, const acm::vulkan::DescriptorSet& set, const acm::Handle& setHandle);
		void dispatch(const acm::Handle& handle, uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ);
		void bufferBarrier(const acm::Handle& handle, const acm::vulkan::Buffer& buffer, const acm::Handle& bufferHandle, acm::ShaderStage srcStage, acm::ShaderStage dstStage);
		void transitionImage(const acm::Handle& handle, const acm::vulkan::Texture& texture, const acm::Handle& textureHandle, acm::ImageLayout from, acm::ImageLayout to);
		void retire(acm::vulkan::Device& owner);

	private:
		acm::CommandPool m_pool;
		acm::RenderTarget m_renderTarget;
		VkCommandBuffer m_commandBuffer{VK_NULL_HANDLE};
	};
} // namespace acm::vulkan
