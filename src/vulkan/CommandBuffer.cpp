#include "archimedes/vulkan/CommandBuffer.h"

#include "archimedes/vulkan/Buffer.h"
#include "archimedes/vulkan/CommandPool.h"
#include "archimedes/vulkan/ComputePipeline.h"
#include "archimedes/vulkan/Convert.h"
#include "archimedes/vulkan/DescriptorSet.h"
#include "archimedes/vulkan/Device.h"
#include "archimedes/vulkan/Pipeline.h"
#include "archimedes/vulkan/RenderTarget.h"
#include "archimedes/vulkan/Texture.h"

#include <utility>

bool acm::vulkan::CommandBuffer::create(acm::vulkan::Device& owner, const acm::CommandPool& pool)
{
	if (!pool.valid() || !pool.native() || &pool.native()->owner() != &owner)
		return false;
	const VkCommandPool vkPool = pool.native()->vkCommandPool(pool.handle());
	if (!vkPool)
		return false;
	VkCommandBufferAllocateInfo allocationInfo = {};
	allocationInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocationInfo.commandPool = vkPool;
	allocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocationInfo.commandBufferCount = 1;
	if (vkAllocateCommandBuffers(owner.vkDevice(), &allocationInfo, &m_commandBuffer) != VK_SUCCESS)
		return false;
	m_pool = pool;
	return true;
}

VkCommandBuffer acm::vulkan::CommandBuffer::vkCommandBuffer(const acm::Handle& handle) const
{
	return accessible(handle) ? m_commandBuffer : VK_NULL_HANDLE;
}

acm::Error acm::vulkan::CommandBuffer::begin(const acm::Handle& handle)
{
	if (!accessible(handle))
		return acm::Error("invalid command buffer");
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	if (vkBeginCommandBuffer(m_commandBuffer, &beginInfo) != VK_SUCCESS)
		return acm::Error("failed to begin command buffer");
	return {};
}

acm::Error acm::vulkan::CommandBuffer::end(const acm::Handle& handle)
{
	if (!accessible(handle))
		return acm::Error("invalid command buffer");
	if (vkEndCommandBuffer(m_commandBuffer) != VK_SUCCESS)
		return acm::Error("failed to end command buffer");
	return {};
}

void acm::vulkan::CommandBuffer::beginRendering(const acm::Handle& handle, const acm::RenderTarget& target, float r, float g, float b, float a)
{
	if (!accessible(handle) || m_renderTarget.valid() || !target.valid() || !target.native() || &owner() != &target.native()->owner())
		return;
	if (!target.native()->beginRendering(target.handle(), m_commandBuffer, r, g, b, a))
		return;
	m_renderTarget = target;
}

void acm::vulkan::CommandBuffer::endRendering(const acm::Handle& handle)
{
	acm::RenderTarget target = std::move(m_renderTarget);
	m_renderTarget.reset();
	if (!target.valid() || !target.native())
		return;
	if (accessible(handle))
		target.native()->endRendering(target.handle(), m_commandBuffer);
}

void acm::vulkan::CommandBuffer::setViewportAndScissor(const acm::Handle& handle, acm::Extent2D extent)
{
	if (!accessible(handle))
		return;
	VkViewport viewport = {};
	viewport.width = float(extent.width);
	viewport.height = float(extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(m_commandBuffer, 0, 1, &viewport);
	VkRect2D scissor = {};
	scissor.extent = {extent.width, extent.height};
	vkCmdSetScissor(m_commandBuffer, 0, 1, &scissor);
}

void acm::vulkan::CommandBuffer::bindPipeline(const acm::Handle& handle, const acm::vulkan::Pipeline& pipeline, const acm::Handle& pipelineHandle)
{
	if (!accessible(handle) || &owner() != &pipeline.owner())
		return;
	const VkPipeline vkPipeline = pipeline.vkPipeline(pipelineHandle);
	if (vkPipeline)
		vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline);
}

void acm::vulkan::CommandBuffer::bindDescriptorSet(const acm::Handle& handle, const acm::vulkan::Pipeline& pipeline, const acm::Handle& pipelineHandle, const acm::vulkan::DescriptorSet& set, const acm::Handle& setHandle, const uint32_t* dynamicOffset)
{
	if (!accessible(handle) || &owner() != &pipeline.owner() || &owner() != &set.owner())
		return;
	const VkPipelineLayout layout = pipeline.vkLayout(pipelineHandle);
	const VkDescriptorSet descriptorSet = set.vkDescriptorSet(setHandle);
	if (!layout || !descriptorSet)
		return;
	const uint32_t dynamicOffsetCount = dynamicOffset ? 1u : 0u;
	vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descriptorSet, dynamicOffsetCount, dynamicOffset);
}

void acm::vulkan::CommandBuffer::draw(const acm::Handle& handle, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
	if (accessible(handle))
		vkCmdDraw(m_commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

void acm::vulkan::CommandBuffer::bindVertexBuffer(const acm::Handle& handle, const acm::vulkan::Buffer& buffer, const acm::Handle& bufferHandle)
{
	if (!accessible(handle) || &owner() != &buffer.owner())
		return;
	const VkBuffer vkBuffer = buffer.vkBuffer(bufferHandle);
	if (!vkBuffer)
		return;
	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(m_commandBuffer, 0, 1, &vkBuffer, &offset);
}

void acm::vulkan::CommandBuffer::bindIndexBuffer(const acm::Handle& handle, const acm::vulkan::Buffer& buffer, const acm::Handle& bufferHandle)
{
	if (!accessible(handle) || &owner() != &buffer.owner())
		return;
	const VkBuffer vkBuffer = buffer.vkBuffer(bufferHandle);
	if (vkBuffer)
		vkCmdBindIndexBuffer(m_commandBuffer, vkBuffer, 0, VK_INDEX_TYPE_UINT32);
}

void acm::vulkan::CommandBuffer::drawIndexed(const acm::Handle& handle, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
	if (accessible(handle))
		vkCmdDrawIndexed(m_commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void acm::vulkan::CommandBuffer::copyTextureToBuffer(const acm::Handle& handle, const acm::vulkan::Texture& texture, const acm::Handle& textureHandle, const acm::vulkan::Buffer& buffer, const acm::Handle& bufferHandle)
{
	if (!accessible(handle) || &owner() != &texture.owner() || &owner() != &buffer.owner())
		return;
	texture.recordCopyToBuffer(textureHandle, m_commandBuffer, buffer, bufferHandle);
}

void acm::vulkan::CommandBuffer::bindComputePipeline(const acm::Handle& handle, const acm::vulkan::ComputePipeline& pipeline, const acm::Handle& pipelineHandle)
{
	if (!accessible(handle) || &owner() != &pipeline.owner())
		return;
	const VkPipeline vkPipeline = pipeline.vkPipeline(pipelineHandle);
	if (vkPipeline)
		vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vkPipeline);
}

void acm::vulkan::CommandBuffer::bindComputeDescriptorSet(const acm::Handle& handle, const acm::vulkan::ComputePipeline& pipeline, const acm::Handle& pipelineHandle, const acm::vulkan::DescriptorSet& set, const acm::Handle& setHandle)
{
	if (!accessible(handle) || &owner() != &pipeline.owner() || &owner() != &set.owner())
		return;
	const VkPipelineLayout layout = pipeline.vkLayout(pipelineHandle);
	const VkDescriptorSet descriptorSet = set.vkDescriptorSet(setHandle);
	if (layout && descriptorSet)
		vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &descriptorSet, 0, nullptr);
}

void acm::vulkan::CommandBuffer::dispatch(const acm::Handle& handle, uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
{
	if (accessible(handle))
		vkCmdDispatch(m_commandBuffer, groupsX, groupsY, groupsZ);
}

void acm::vulkan::CommandBuffer::bufferBarrier(const acm::Handle& handle, const acm::vulkan::Buffer& buffer, const acm::Handle& bufferHandle, acm::ShaderStage srcStage, acm::ShaderStage dstStage)
{
	if (!accessible(handle) || &owner() != &buffer.owner())
		return;
	const VkBuffer vkBuffer = buffer.vkBuffer(bufferHandle);
	if (!vkBuffer)
		return;
	VkBufferMemoryBarrier2 barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	barrier.srcStageMask = acm::vulkan::toVkPipelineStage(srcStage);
	barrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
	barrier.dstStageMask = acm::vulkan::toVkPipelineStage(dstStage);
	barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer = vkBuffer;
	barrier.size = VK_WHOLE_SIZE;
	VkDependencyInfo dependency = {};
	dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependency.bufferMemoryBarrierCount = 1;
	dependency.pBufferMemoryBarriers = &barrier;
	vkCmdPipelineBarrier2(m_commandBuffer, &dependency);
}

void acm::vulkan::CommandBuffer::transitionImage(const acm::Handle& handle, const acm::vulkan::Texture& texture, const acm::Handle& textureHandle, acm::ImageLayout from, acm::ImageLayout to)
{
	if (!accessible(handle) || &owner() != &texture.owner())
		return;
	texture.recordTransition(textureHandle, m_commandBuffer, from, to);
}

void acm::vulkan::CommandBuffer::retire(acm::vulkan::Device& owner)
{
	m_renderTarget.reset();
	const VkCommandBuffer commandBuffer = std::exchange(m_commandBuffer, VK_NULL_HANDLE);
	const VkCommandPool commandPool = m_pool.valid() ? m_pool.native()->vkCommandPool(m_pool.handle()) : VK_NULL_HANDLE;
	if (commandPool && commandBuffer)
	{
		const VkDevice device = owner.vkDevice();
		const VkCommandPool retiredPool = commandPool;
		const VkCommandBuffer retiredBuffer = commandBuffer;
		owner.enqueueDestroy([device, retiredPool, retiredBuffer]
							 { vkFreeCommandBuffers(device, retiredPool, 1, &retiredBuffer); });
	}
	m_pool.reset();
}
