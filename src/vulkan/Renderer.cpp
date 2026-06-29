#include "archimedes/vulkan/Renderer.h"

#include "archimedes/acmRenderer.h"
#include "archimedes/acmRenderTarget.h"
#include "archimedes/vulkan/CommandBuffer.h"
#include "archimedes/vulkan/Device.h"
#include "archimedes/vulkan/SwapChain.h"

#include <limits>
#include <utility>

bool acm::vulkan::Renderer::create(acm::vulkan::Device& owner, const acm::SwapChain& swapChain)
{
	if (!swapChain.valid() || &swapChain.native()->owner() != &owner)
		return false;
	m_swapChain = swapChain;
	m_commandPool = owner.createCommandPool();
	if (!m_commandPool.valid())
		return false;
	m_commandBuffers.reserve(acm::Renderer::MaxFramesInFlight);
	for (uint32_t index = 0; index < acm::Renderer::MaxFramesInFlight; ++index)
	{
		acm::CommandBuffer commandBuffer = owner.allocateCommandBuffer(m_commandPool);
		if (!commandBuffer.valid())
			return false;
		m_commandBuffers.push_back(std::move(commandBuffer));
	}

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	m_frames.resize(acm::Renderer::MaxFramesInFlight);
	for (Frame& frame : m_frames)
		if (vkCreateSemaphore(owner.vkDevice(), &semaphoreInfo, nullptr, &frame.imageAvailable) != VK_SUCCESS ||
			vkCreateSemaphore(owner.vkDevice(), &semaphoreInfo, nullptr, &frame.renderFinished) != VK_SUCCESS ||
			vkCreateFence(owner.vkDevice(), &fenceInfo, nullptr, &frame.inFlight) != VK_SUCCESS)
			return false;
	return true;
}

acm::Error acm::vulkan::Renderer::render(const acm::Handle& handle, const std::function<void(acm::CommandBuffer&, uint32_t)>& prePass, const std::function<void(acm::CommandBuffer&, uint32_t)>& record)
{
	if (!accessible(handle) || !m_swapChain.valid())
		return acm::Error("invalid renderer");
	if (m_needsRecreate)
	{
		if (!m_swapChain.recreate())
			return {};
		m_needsRecreate = false;
	}

	Frame& frame = m_frames[m_currentFrame];
	vkWaitForFences(owner().vkDevice(), 1, &frame.inFlight, VK_TRUE, std::numeric_limits<uint64_t>::max());
	uint32_t imageIndex = 0;
	const VkResult acquire = m_swapChain.native()->acquireNextImage(m_swapChain.handle(), frame.imageAvailable, imageIndex);
	if (acquire == VK_ERROR_OUT_OF_DATE_KHR)
	{
		m_needsRecreate = true;
		return {};
	}
	if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR)
		return acm::Error("failed to acquire swapchain image");

	owner().beginFrame();
	const uint64_t currentFrame = owner().currentFrame();
	if (currentFrame > acm::Renderer::MaxFramesInFlight)
		owner().collectGarbage(currentFrame - acm::Renderer::MaxFramesInFlight);
	acm::RenderTarget target = m_swapChain.getRenderTarget(imageIndex);
	if (!target.valid())
		return acm::Error("acquired swapchain image has no render target");
	acm::CommandBuffer& commandBuffer = m_commandBuffers[m_currentFrame];
	if (acm::Error error = commandBuffer.begin())
		return error;
	if (prePass)
		prePass(commandBuffer, uint32_t(m_currentFrame));
	commandBuffer.beginRendering(target);
	commandBuffer.setViewportAndScissor(target.getExtent());
	if (record)
		record(commandBuffer, uint32_t(m_currentFrame));
	commandBuffer.endRendering();
	if (acm::Error error = commandBuffer.end())
		return error;

	const VkCommandBuffer vkCommand = commandBuffer.native()->vkCommandBuffer(commandBuffer.handle());
	const VkSwapchainKHR vkSwapChain = m_swapChain.native()->vkSwapChain(m_swapChain.handle());
	if (!vkCommand || !vkSwapChain)
		return acm::Error("renderer resources became invalid");
	if (acm::Error error = owner().submitFrame(vkCommand, frame.imageAvailable, frame.renderFinished, frame.inFlight, vkSwapChain, imageIndex, m_needsRecreate))
		return error;
	m_currentFrame = (m_currentFrame + 1) % acm::Renderer::MaxFramesInFlight;
	return {};
}

void acm::vulkan::Renderer::retire(acm::vulkan::Device& owner)
{
	const VkDevice device = owner.vkDevice();
	for (const Frame& frame : m_frames)
	{
		if (frame.renderFinished)
			owner.enqueueDestroy([device, semaphore = frame.renderFinished]
								 { vkDestroySemaphore(device, semaphore, nullptr); });
		if (frame.imageAvailable)
			owner.enqueueDestroy([device, semaphore = frame.imageAvailable]
								 { vkDestroySemaphore(device, semaphore, nullptr); });
		if (frame.inFlight)
			owner.enqueueDestroy([device, fence = frame.inFlight]
								 { vkDestroyFence(device, fence, nullptr); });
	}
	m_frames.clear();
	m_commandBuffers.clear();
	m_commandPool.reset();
	m_swapChain.reset();
	m_currentFrame = 0;
	m_needsRecreate = false;
}
