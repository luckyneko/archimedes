#include "archimedes/acmRenderer.h"
#include "archimedes/acmCommandBuffer.h"
#include "archimedes/acmCommandPool.h"
#include "archimedes/acmDevice.h"
#include "archimedes/acmRenderTarget.h"
#include "archimedes/acmSwapChain.h"
#include <limits>
#include <mutex>
#include <vector>
#include <vulkan/vulkan.h>

namespace
{
	constexpr size_t MAX_FRAMES_IN_FLIGHT = acm::Renderer::MaxFramesInFlight;
}

struct acm::Renderer::impl
{
	acm::Device device;
	acm::SwapChain swapChain;
	acm::CommandPool commandPool;
	std::vector<acm::CommandBuffer> commandBuffers; // one per frame-in-flight

	struct FrameSync
	{
		VkSemaphore imageAvailable{VK_NULL_HANDLE};
		VkSemaphore renderFinished{VK_NULL_HANDLE};
		VkFence inFlight{VK_NULL_HANDLE};
	};
	std::vector<FrameSync> frames;
	size_t currentFrame{0};
	bool needsRecreate{false}; // a prior acquire/present reported the swapchain out of date

	~impl()
	{
		if (!device.valid())
			return;

		// Defer sync-object teardown onto the device's frame-fenced queue (flushed
		// after the device waits idle). The command pool is an acm handle and tears
		// itself down the same way; its buffers go with it.
		VkDevice dev = device.vkDevice();
		for (auto& frame : frames)
		{
			if (frame.renderFinished)
			{
				VkSemaphore s = frame.renderFinished;
				device.enqueueDestroy([dev, s]
									  { vkDestroySemaphore(dev, s, nullptr); });
			}
			if (frame.imageAvailable)
			{
				VkSemaphore s = frame.imageAvailable;
				device.enqueueDestroy([dev, s]
									  { vkDestroySemaphore(dev, s, nullptr); });
			}
			if (frame.inFlight)
			{
				VkFence f = frame.inFlight;
				device.enqueueDestroy([dev, f]
									  { vkDestroyFence(dev, f, nullptr); });
			}
		}
	}
};

acm::Renderer::Renderer(acm::Device device, acm::SwapChain swapChain)
	: m()
{
	auto impl = std::make_shared<acm::Renderer::impl>();
	impl->device = device;
	impl->swapChain = swapChain;

	impl->commandPool = device.createCommandPool();
	if (!impl->commandPool.valid())
	{
		m_error = acm::Error("failed to create renderer command pool");
		return;
	}

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		acm::CommandBuffer cb = impl->commandPool.allocate();
		if (!cb.valid())
		{
			m_error = acm::Error("failed to allocate renderer command buffer");
			return;
		}
		impl->commandBuffers.push_back(cb);
	}

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkDevice dev = device.vkDevice();
	impl->frames.resize(MAX_FRAMES_IN_FLIGHT);
	for (auto& frame : impl->frames)
	{
		if (vkCreateSemaphore(dev, &semaphoreInfo, nullptr, &frame.imageAvailable) != VK_SUCCESS ||
			vkCreateSemaphore(dev, &semaphoreInfo, nullptr, &frame.renderFinished) != VK_SUCCESS ||
			vkCreateFence(dev, &fenceInfo, nullptr, &frame.inFlight) != VK_SUCCESS)
		{
			m_error = acm::Error("failed to create renderer sync objects");
			return;
		}
	}

	m = impl;
}

acm::Error acm::Renderer::render(const std::function<void(acm::CommandBuffer, uint32_t)>& record)
{
	return render({}, record); // no pre-pass
}

acm::Error acm::Renderer::render(const std::function<void(acm::CommandBuffer, uint32_t)>& prePass, const std::function<void(acm::CommandBuffer, uint32_t)>& record)
{
	// The device mutex serializes the bits that touch device-shared state — the one
	// VkQueue and the frame/graveyard bookkeeping — so several Renderers can run this on
	// their own threads. Per-renderer work (fences, acquire, recording) stays unlocked
	// and runs concurrently. Uncontended for a single window.
	std::mutex& deviceMutex = m->device.deviceMutex();

	// A prior frame saw the swapchain go out of date: rebuild before doing anything
	// else. A zero-sized (minimized) surface fails the rebuild — skip this frame and
	// retry on the next tick. recreate() waits the device idle, which must not race
	// another thread's submit, so it holds the device lock.
	if (m->needsRecreate)
	{
		std::lock_guard<std::mutex> lock(deviceMutex);
		if (!m->swapChain.recreate())
			return acm::Error{}; // minimized — caller retries
		m->needsRecreate = false;
	}

	auto& frame = m->frames[m->currentFrame];
	VkDevice dev = m->device.vkDevice();

	// Wait on this slot's fence before touching its command buffer / semaphores.
	vkWaitForFences(dev, 1, &frame.inFlight, VK_TRUE, std::numeric_limits<uint64_t>::max());

	uint32_t imageIndex = 0;
	VkResult acquire = vkAcquireNextImageKHR(dev, m->swapChain.vkSwapChain(), std::numeric_limits<uint64_t>::max(), frame.imageAvailable, VK_NULL_HANDLE, &imageIndex);
	if (acquire == VK_ERROR_OUT_OF_DATE_KHR)
	{
		// Nothing was acquired (imageAvailable unsignaled, fence still signaled):
		// recreate next frame without advancing or resetting state.
		m->needsRecreate = true;
		return acm::Error{};
	}
	// VK_SUBOPTIMAL_KHR still yields a usable image — render, then recreate after present.

	// Reset the fence only now that we know we will submit; resetting it on a frame
	// we then skip would leave it unsignaled and hang the next wait.
	vkResetFences(dev, 1, &frame.inFlight);

	{
		// Frame/graveyard bookkeeping is device-shared — lock it. This slot's fence
		// signaled, so the frame submitted MAX_FRAMES_IN_FLIGHT ago has retired:
		// anything tagged at/before it is safe to free.
		std::lock_guard<std::mutex> lock(deviceMutex);
		m->device.beginFrame();
		const uint64_t cur = m->device.currentFrame();
		if (cur > MAX_FRAMES_IN_FLIGHT)
			m->device.collectGarbage(cur - MAX_FRAMES_IN_FLIGHT);
	}

	acm::RenderTarget target = m->swapChain.getRenderTarget(imageIndex);
	acm::CommandBuffer cmd = m->commandBuffers[m->currentFrame];
	if (auto err = cmd.begin())
		return err;
	// Pre-pass: compute / barriers / transitions, recorded before (and outside) the render
	// pass into the same command buffer, so their results feed the draws with no extra submit.
	if (prePass)
		prePass(cmd, uint32_t(m->currentFrame));
	cmd.beginRenderPass(target);
	cmd.setViewportAndScissor(target.getExtent());
	record(cmd, uint32_t(m->currentFrame));
	cmd.endRenderPass();
	if (auto err = cmd.end())
		return err;

	VkCommandBuffer vkcb = cmd.vkCommandBuffer();
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &frame.imageAvailable;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &vkcb;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &frame.renderFinished;

	VkSwapchainKHR swapChains[] = {m->swapChain.vkSwapChain()};
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &frame.renderFinished;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;

	// Submit + present touch the one VkQueue, which must be externally synchronized
	// across threads — both go under the device lock.
	{
		std::lock_guard<std::mutex> lock(deviceMutex);
		if (vkQueueSubmit(m->device.vkQueue(), 1, &submitInfo, frame.inFlight) != VK_SUCCESS)
			return acm::Error("failed to submit draw command buffer");

		VkResult present = vkQueuePresentKHR(m->device.vkQueue(), &presentInfo);
		if (present == VK_ERROR_OUT_OF_DATE_KHR || present == VK_SUBOPTIMAL_KHR)
			m->needsRecreate = true;
	}

	m->currentFrame = (m->currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	return acm::Error{};
}
