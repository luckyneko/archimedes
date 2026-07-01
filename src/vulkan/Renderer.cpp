#include "archimedes/vulkan/Renderer.h"

#include "archimedes/acmRenderer.h"
#include "archimedes/acmRenderTarget.h"
#include "archimedes/vulkan/CommandBuffer.h"
#include "archimedes/vulkan/Device.h"
#include "archimedes/vulkan/SwapChain.h"

#include <limits>
#include <utility>

acm::vulkan::Renderer::Renderer(acm::vulkan::Device& owner, const acm::SwapChain& swapChain)
{
	if (!swapChain.valid() || !swapChain.native() || &swapChain.native()->owner() != &owner)
	{
		m_error = acm::Error("failed to create renderer from invalid swapchain");
		return;
	}
	m_owner = &owner;
	m_swapChain = swapChain;
	m_commandPool = owner.createCommandPool();
	if (!m_commandPool.valid())
	{
		m_error = acm::Error("failed to create renderer command pool");
		return;
	}
	m_commandBuffers.reserve(acm::Renderer::MaxFramesInFlight);
	for (uint32_t index = 0; index < acm::Renderer::MaxFramesInFlight; ++index)
	{
		acm::CommandBuffer commandBuffer = owner.allocateCommandBuffer(m_commandPool);
		if (!commandBuffer.valid())
		{
			m_error = acm::Error("failed to create renderer command buffer");
			return;
		}
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
		{
			m_error = acm::Error("failed to create renderer frame synchronization");
			return;
		}
}

acm::vulkan::Renderer::~Renderer()
{
	release();
}

acm::vulkan::Renderer::Renderer(Renderer&& other) noexcept
{
	*this = std::move(other);
}

acm::vulkan::Renderer& acm::vulkan::Renderer::operator=(Renderer&& other) noexcept
{
	if (this == &other)
		return *this;
	release();
	m_owner = std::exchange(other.m_owner, nullptr);
	m_swapChain = std::move(other.m_swapChain);
	m_commandPool = std::move(other.m_commandPool);
	m_commandBuffers = std::move(other.m_commandBuffers);
	m_frames = std::move(other.m_frames);
	m_currentFrame = std::exchange(other.m_currentFrame, 0);
	m_needsRecreate = std::exchange(other.m_needsRecreate, false);
	m_error = std::move(other.m_error);
	return *this;
}

acm::Error acm::vulkan::Renderer::render(const std::function<void(acm::CommandBuffer&, uint32_t)>& prePass, const std::function<void(acm::CommandBuffer&, uint32_t)>& record)
{
	if (!m_swapChain.valid())
		return acm::Error("invalid renderer");
	if (m_needsRecreate)
	{
		if (!m_swapChain.recreate())
			return {};
		m_needsRecreate = false;
	}

	Frame& frame = m_frames[m_currentFrame];
	vkWaitForFences(owner().vkDevice(), 1, &frame.inFlight, VK_TRUE, std::numeric_limits<uint64_t>::max());
	if (frame.submissionSerial != 0)
	{
		owner().collectGarbage(frame.submissionSerial);
		frame.submissionSerial = 0;
	}
	uint32_t imageIndex = 0;
	const VkResult acquire = m_swapChain.native()->acquireNextImage(frame.imageAvailable, imageIndex);
	if (acquire == VK_ERROR_OUT_OF_DATE_KHR)
	{
		m_needsRecreate = true;
		return {};
	}
	if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR)
		return acm::Error("failed to acquire swapchain image");

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

	const VkCommandBuffer vkCommand = commandBuffer.native()->vkCommandBuffer();
	const VkSwapchainKHR vkSwapChain = m_swapChain.native()->vkSwapChain();
	if (!vkCommand || !vkSwapChain)
		return acm::Error("renderer resources became invalid");
	if (acm::Error error = owner().submitFrame(vkCommand, frame.imageAvailable, frame.renderFinished, frame.inFlight, vkSwapChain, imageIndex, m_needsRecreate, frame.submissionSerial))
		return error;
	m_currentFrame = (m_currentFrame + 1) % acm::Renderer::MaxFramesInFlight;
	return {};
}

void acm::vulkan::Renderer::release()
{
	acm::vulkan::Device* owner = std::exchange(m_owner, nullptr);
	const VkDevice device = owner ? owner->vkDevice() : VK_NULL_HANDLE;
	for (const Frame& frame : m_frames)
	{
		if (device && frame.renderFinished)
			vkDestroySemaphore(device, frame.renderFinished, nullptr);
		if (device && frame.imageAvailable)
			vkDestroySemaphore(device, frame.imageAvailable, nullptr);
		if (device && frame.inFlight)
			vkDestroyFence(device, frame.inFlight, nullptr);
	}
	m_frames.clear();
	m_commandBuffers.clear();
	m_commandPool.reset();
	m_swapChain.reset();
	m_currentFrame = 0;
	m_needsRecreate = false;
}
