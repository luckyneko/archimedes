#include "archimedes/acmCommandBuffer.h"

#include "archimedes/acmBuffer.h"
#include "archimedes/acmCommandPool.h"
#include "archimedes/acmComputePipeline.h"
#include "archimedes/acmDescriptorSet.h"
#include "archimedes/acmPipeline.h"
#include "archimedes/acmRenderTarget.h"
#include "archimedes/acmTexture.h"

#include <vulkan/vulkan.h>

struct acm::CommandBuffer::impl
{
	acm::CommandPool pool; // keeps the owning VkCommandPool (and our buffer) alive
	VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
	// No destructor work: the pool frees its command buffers on teardown.
};

acm::CommandBuffer::CommandBuffer(acm::CommandPool pool, VkCommandBuffer commandBuffer)
	: m()
{
	if (commandBuffer == VK_NULL_HANDLE)
		return;

	auto impl = std::make_shared<acm::CommandBuffer::impl>();
	impl->pool = pool;
	impl->commandBuffer = commandBuffer;
	m = impl;
}

acm::Error acm::CommandBuffer::begin()
{
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	if (vkBeginCommandBuffer(m->commandBuffer, &beginInfo) != VK_SUCCESS)
		return acm::Error("failed to begin command buffer");
	return acm::Error{};
}

acm::Error acm::CommandBuffer::end()
{
	if (vkEndCommandBuffer(m->commandBuffer) != VK_SUCCESS)
		return acm::Error("failed to end command buffer");
	return acm::Error{};
}

void acm::CommandBuffer::beginRenderPass(acm::RenderTarget target, float r, float g, float b, float a)
{
	const acm::Extent2D extent = target.getExtent();

	// Clear values are positional, matching the render pass attachments: color at 0,
	// then (under MSAA) the resolve attachment at 1, then depth — so the depth clear's
	// index shifts when multisampled. The resolve slot needs no clear (DONT_CARE load).
	VkClearValue clears[3] = {};
	clears[0].color = {{r, g, b, a}};
	uint32_t clearCount = 1;
	if (target.hasDepth())
	{
		const uint32_t depthIndex = target.isMultisampled() ? 2u : 1u;
		clears[depthIndex].depthStencil = {1.0f, 0};
		clearCount = depthIndex + 1;
	}

	VkRenderPassBeginInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = target.vkRenderPass();
	renderPassInfo.framebuffer = target.vkFramebuffer();
	renderPassInfo.renderArea.extent = {extent.width, extent.height};
	renderPassInfo.clearValueCount = clearCount;
	renderPassInfo.pClearValues = clears;

	vkCmdBeginRenderPass(m->commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void acm::CommandBuffer::endRenderPass()
{
	vkCmdEndRenderPass(m->commandBuffer);
}

void acm::CommandBuffer::setViewportAndScissor(acm::Extent2D extent)
{
	VkViewport viewport = {};
	viewport.width = float(extent.width);
	viewport.height = float(extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(m->commandBuffer, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.extent = {extent.width, extent.height};
	vkCmdSetScissor(m->commandBuffer, 0, 1, &scissor);
}

void acm::CommandBuffer::bindPipeline(acm::Pipeline pipeline)
{
	vkCmdBindPipeline(m->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.vkPipeline());
}

void acm::CommandBuffer::bindDescriptorSet(acm::Pipeline pipeline, acm::DescriptorSet set)
{
	VkDescriptorSet vkSet = set.vkDescriptorSet();
	vkCmdBindDescriptorSets(m->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.vkPipelineLayout(), 0, 1, &vkSet, 0, nullptr);
}

void acm::CommandBuffer::bindDescriptorSet(acm::Pipeline pipeline, acm::DescriptorSet set, uint32_t dynamicOffset)
{
	VkDescriptorSet vkSet = set.vkDescriptorSet();
	vkCmdBindDescriptorSets(m->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.vkPipelineLayout(), 0, 1, &vkSet, 1, &dynamicOffset);
}

void acm::CommandBuffer::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
	vkCmdDraw(m->commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

void acm::CommandBuffer::bindVertexBuffer(acm::Buffer buffer)
{
	VkBuffer buf = buffer.vkBuffer();
	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(m->commandBuffer, 0, 1, &buf, &offset);
}

void acm::CommandBuffer::bindIndexBuffer(acm::Buffer buffer)
{
	vkCmdBindIndexBuffer(m->commandBuffer, buffer.vkBuffer(), 0, VK_INDEX_TYPE_UINT32);
}

void acm::CommandBuffer::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
	vkCmdDrawIndexed(m->commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void acm::CommandBuffer::copyTextureToBuffer(acm::Texture texture, acm::Buffer buffer)
{
	const acm::Extent2D extent = texture.getExtent();

	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;	  // tightly packed
	region.bufferImageHeight = 0; // tightly packed
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = {0, 0, 0};
	region.imageExtent = {extent.width, extent.height, 1};

	vkCmdCopyImageToBuffer(m->commandBuffer, texture.vkImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer.vkBuffer(), 1, &region);
}

void acm::CommandBuffer::bindComputePipeline(acm::ComputePipeline pipeline)
{
	vkCmdBindPipeline(m->commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.vkPipeline());
}

void acm::CommandBuffer::bindComputeDescriptorSet(acm::ComputePipeline pipeline, acm::DescriptorSet set)
{
	VkDescriptorSet vkSet = set.vkDescriptorSet();
	vkCmdBindDescriptorSets(m->commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.vkPipelineLayout(), 0, 1, &vkSet, 0, nullptr);
}

void acm::CommandBuffer::dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
{
	vkCmdDispatch(m->commandBuffer, groupsX, groupsY, groupsZ);
}

namespace
{
	// The pipeline stage(s) a ShaderStage flag set runs in.
	VkPipelineStageFlags toPipelineStage(acm::ShaderStage stage)
	{
		VkPipelineStageFlags flags = 0;
		if (uint32_t(stage) & uint32_t(acm::ShaderStage::Vertex))
			flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
		if (uint32_t(stage) & uint32_t(acm::ShaderStage::Fragment))
			flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		if (uint32_t(stage) & uint32_t(acm::ShaderStage::Compute))
			flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		return flags ? flags : VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	}
} // namespace

void acm::CommandBuffer::bufferBarrier(acm::Buffer buffer, acm::ShaderStage srcStage, acm::ShaderStage dstStage)
{
	VkBufferMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	// Conservative: cover read-after-write, write-after-read, and write-after-write.
	barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer = buffer.vkBuffer();
	barrier.offset = 0;
	barrier.size = VK_WHOLE_SIZE;

	vkCmdPipelineBarrier(m->commandBuffer, toPipelineStage(srcStage), toPipelineStage(dstStage), 0, 0, nullptr, 1, &barrier, 0, nullptr);
}

namespace
{
	// Conservative (layout, access, stage) for an image transition. Coarse but correct —
	// transitions are setup/one-shot here, not a hot path.
	struct LayoutInfo
	{
		VkImageLayout layout;
		VkAccessFlags access;
		VkPipelineStageFlags stage;
	};

	LayoutInfo layoutInfo(acm::ImageLayout l)
	{
		switch (l)
		{
			case acm::ImageLayout::General:
				return {VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};
			case acm::ImageLayout::ShaderReadOnly:
				return {VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};
			case acm::ImageLayout::TransferSrc:
				return {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT};
			case acm::ImageLayout::TransferDst:
				return {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT};
			case acm::ImageLayout::Undefined:
				break;
		}
		return {VK_IMAGE_LAYOUT_UNDEFINED, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
	}
} // namespace

void acm::CommandBuffer::transitionImage(acm::Texture texture, acm::ImageLayout from, acm::ImageLayout to)
{
	const LayoutInfo f = layoutInfo(from);
	const LayoutInfo t = layoutInfo(to);

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = f.layout;
	barrier.newLayout = t.layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = texture.vkImage();
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = texture.mipLevels();
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = f.access;
	barrier.dstAccessMask = t.access;

	vkCmdPipelineBarrier(m->commandBuffer, f.stage, t.stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

VkCommandBuffer acm::CommandBuffer::vkCommandBuffer() const
{
	return m->commandBuffer;
}
