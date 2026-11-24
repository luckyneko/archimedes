/*
 *  Created by LuckyNeko on 24/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/vulkan/Renderer.h"

#include "archimedes/acmRenderer.h"
#include "archimedes/acmRenderTarget.h"
#include "archimedes/vulkan/CommandBuffer.h"
#include "archimedes/vulkan/Device.h"
#include "archimedes/vulkan/SwapChain.h"

#include <limits>
#include <utility>

namespace acm::vulkan
{

	// -----------------------------------------------------------------------------
	// Lifetime
	// -----------------------------------------------------------------------------

	Renderer::Renderer(Device& owner, const acm::SwapChain& swapChain)
	{
		if (!swapChain.valid() || !swapChain.backend() || &swapChain.backend()->owner() != &owner)
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
		m_frames.resize(acm::Renderer::MaxFramesInFlight);
		for (Frame& frame : m_frames)
			if (vkCreateSemaphore(owner.vkDevice(), &semaphoreInfo, nullptr, &frame.imageAvailable) != VK_SUCCESS ||
				vkCreateFence(owner.vkDevice(), &fenceInfo, nullptr, &frame.inFlight) != VK_SUCCESS)
			{
				m_error = acm::Error("failed to create renderer frame synchronization");
				return;
			}
		if (acm::Error error = rebuildPresentSemaphores(*swapChain.backend()))
			m_error = error;
	}

	Renderer::~Renderer()
	{
		release();
	}

	Renderer::Renderer(Renderer&& other) noexcept
	{
		*this = std::move(other);
	}

	Renderer& Renderer::operator=(Renderer&& other) noexcept
	{
		if (this == &other)
			return *this;
		release();
		m_owner = std::exchange(other.m_owner, nullptr);
		m_swapChain = std::move(other.m_swapChain);
		m_commandPool = std::move(other.m_commandPool);
		m_commandBuffers = std::move(other.m_commandBuffers);
		m_frames = std::move(other.m_frames);
		m_presentSemaphores = std::move(other.m_presentSemaphores);
		m_presentGeneration = std::exchange(other.m_presentGeneration, 0);
		m_currentFrame = std::exchange(other.m_currentFrame, 0);
		m_needsRecreate = std::exchange(other.m_needsRecreate, false);
		m_error = std::move(other.m_error);
		return *this;
	}

	// -----------------------------------------------------------------------------
	// Frames
	// -----------------------------------------------------------------------------

	acm::Error Renderer::render(const std::function<void(acm::CommandBuffer&, uint32_t)>& prePass, const std::function<void(acm::CommandBuffer&, uint32_t)>& record)
	{
		if (!m_swapChain.valid())
			return acm::Error("invalid renderer");
		if (m_needsRecreate)
		{
			if (!m_swapChain.recreate())
				return {};
			m_needsRecreate = false;
		}
		SwapChain& swapChain = *m_swapChain.backend();
		if (swapChain.generation() != m_presentGeneration)
		{
			if (acm::Error error = rebuildPresentSemaphores(swapChain))
				return error;
		}

		Frame& frame = m_frames[m_currentFrame];
		if (frame.pending)
		{
			vkWaitForFences(owner().vkDevice(), 1, &frame.inFlight, VK_TRUE, std::numeric_limits<uint64_t>::max());
			frame.pending = false;
			owner().collectGarbage(frame.submissionSerial);
		}
		uint32_t imageIndex = 0;
		const VkResult acquire = swapChain.acquireNextImage(frame.imageAvailable, imageIndex);
		if (acquire == VK_ERROR_OUT_OF_DATE_KHR)
		{
			m_needsRecreate = true;
			return {};
		}
		if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR)
			return acm::Error("failed to acquire swapchain image");

		// The image is acquired and imageAvailable will be signalled, so from here the frame
		// is always submitted and the slot always advances, whatever recording does: an early
		// return would hand the next acquire a semaphore that is still signalled and never
		// give the image back.
		const uint32_t frameIndex = uint32_t(m_currentFrame);
		m_currentFrame = (m_currentFrame + 1) % acm::Renderer::MaxFramesInFlight;
		acm::CommandBuffer& commandBuffer = m_commandBuffers[frameIndex];
		const acm::RenderTarget target = swapChain.renderTarget(imageIndex);
		acm::Error frameError = recordFrame(commandBuffer, target, frameIndex, prePass, record);
		const bool presentable = !frameError || recordClearFrame(commandBuffer, target);

		FrameSubmission submission;
		submission.imageAvailable = frame.imageAvailable;
		submission.inFlight = frame.inFlight;
		if (presentable)
		{
			submission.commandBuffer = commandBuffer.backend() ? commandBuffer.backend()->vkCommandBuffer() : VK_NULL_HANDLE;
			submission.renderFinished = imageIndex < m_presentSemaphores.size() ? m_presentSemaphores[imageIndex] : VK_NULL_HANDLE;
			submission.swapChain = swapChain.vkSwapChain();
			submission.imageIndex = imageIndex;
			if (!frameError && (!submission.commandBuffer || !submission.renderFinished || !submission.swapChain))
				frameError = acm::Error("renderer resources became invalid");
		}
		FrameSubmitResult result;
		const acm::Error submitError = owner().submitFrame(submission, result);
		frame.pending = result.submitted;
		frame.submissionSerial = result.serial;
		if (result.needsRecreate)
			m_needsRecreate = true;
		return frameError ? frameError : submitError;
	}

	// -----------------------------------------------------------------------------
	// Internals
	// -----------------------------------------------------------------------------

	void Renderer::release()
	{
		Device* owner = std::exchange(m_owner, nullptr);
		const VkDevice device = owner ? owner->vkDevice() : VK_NULL_HANDLE;
		// No fence covers a present's wait on its semaphore; only an idle device does.
		if (device && !m_presentSemaphores.empty())
			owner->waitIdle();
		for (const VkSemaphore semaphore : m_presentSemaphores)
		{
			if (device && semaphore)
				vkDestroySemaphore(device, semaphore, nullptr);
		}
		for (const Frame& frame : m_frames)
		{
			if (device && frame.imageAvailable)
				vkDestroySemaphore(device, frame.imageAvailable, nullptr);
			if (device && frame.inFlight)
				vkDestroyFence(device, frame.inFlight, nullptr);
		}
		m_presentSemaphores.clear();
		m_presentGeneration = 0;
		m_frames.clear();
		m_commandBuffers.clear();
		m_commandPool.reset();
		m_swapChain.reset();
		m_currentFrame = 0;
		m_needsRecreate = false;
	}

	acm::Error Renderer::rebuildPresentSemaphores(const SwapChain& swapChain)
	{
		// Every swapchain rebuild after the first follows a device-idle wait, which also
		// finished the presents waiting on the old semaphores.
		const VkDevice device = owner().vkDevice();
		for (const VkSemaphore semaphore : m_presentSemaphores)
			vkDestroySemaphore(device, semaphore, nullptr);
		m_presentSemaphores.assign(swapChain.renderTargetCount(), VK_NULL_HANDLE);
		VkSemaphoreCreateInfo semaphoreInfo = {};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		for (VkSemaphore& semaphore : m_presentSemaphores)
		{
			if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS)
				return acm::Error("failed to create renderer present semaphores");
		}
		m_presentGeneration = swapChain.generation();
		return {};
	}

	acm::Error Renderer::recordFrame(acm::CommandBuffer& commandBuffer, const acm::RenderTarget& target, uint32_t frameIndex, const std::function<void(acm::CommandBuffer&, uint32_t)>& prePass, const std::function<void(acm::CommandBuffer&, uint32_t)>& record)
	{
		if (!target.valid())
			return acm::Error("acquired swapchain image has no render target");
		if (acm::Error error = commandBuffer.begin())
			return error;
		if (prePass)
			prePass(commandBuffer, frameIndex);
		acm::Error error = commandBuffer.error();
		if (!error)
			error = commandBuffer.beginRendering(target);
		if (!error)
		{
			commandBuffer.setViewportAndScissor(target.extent());
			if (record)
				record(commandBuffer, frameIndex);
			error = commandBuffer.error();
		}
		// Close the open scope: the frame's, or one a callback left open after closing the
		// frame's itself. None is open when the pre-pass failed or beginRendering did. A
		// failure closing it (its target retired mid-frame) is the frame's error unless an
		// earlier one came first.
		if (renderingActive(commandBuffer))
		{
			const acm::Error endRenderingError = commandBuffer.endRendering();
			if (!error)
				error = endRenderingError;
		}
		const acm::Error endError = commandBuffer.end();
		return error ? error : endError;
	}

	bool Renderer::recordClearFrame(acm::CommandBuffer& commandBuffer, const acm::RenderTarget& target)
	{
		// Replaces a failed recording with a bare clear, which still leaves the acquired image
		// in its present layout.
		if (!target.valid() || commandBuffer.begin())
			return false;
		bool cleared = !commandBuffer.beginRendering(target);
		if (cleared)
			cleared = !commandBuffer.endRendering();
		return !commandBuffer.end() && cleared;
	}

	bool Renderer::renderingActive(const acm::CommandBuffer& commandBuffer)
	{
		const CommandBuffer* backend = commandBuffer.backend();
		return backend && backend->renderingActive();
	}

} // namespace acm::vulkan
