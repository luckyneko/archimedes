/*
 *  Created by LuckyNeko on 24/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmCommandBuffer.h"
#include "archimedes/acmCommandPool.h"
#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmSwapChain.h"

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace acm::vulkan
{
	class Device;
	class SwapChain;

	// Move-only swapchain frame orchestrator. Owns per-frame synchronization and command
	// buffers, and submits through Device's serialized queue path.
	class Renderer
	{
	public:
		// Lifetime
		Renderer() = default;
		Renderer(acm::vulkan::Device& owner, const acm::SwapChain& swapChain);
		~Renderer();
		Renderer(const Renderer&) = delete;
		Renderer& operator=(const Renderer&) = delete;
		Renderer(Renderer&& other) noexcept;
		Renderer& operator=(Renderer&& other) noexcept;

		// State
		acm::vulkan::Device& owner() const { return *m_owner; }
		bool valid() const { return m_owner && !m_frames.empty() && m_error.ok(); }
		acm::Error error() const { return m_error; }

		// Frames
		// Once an image is acquired the frame is always submitted and the frame slot advances.
		// A recording error is returned after the frame is re-recorded as a plain clear and
		// presented, so the next render() starts clean.
		acm::Error render(const std::function<void(acm::CommandBuffer&, uint32_t)>& prePass, const std::function<void(acm::CommandBuffer&, uint32_t)>& record);

	private:
		struct Frame
		{
			VkSemaphore imageAvailable{VK_NULL_HANDLE};
			VkFence inFlight{VK_NULL_HANDLE};
			uint64_t submissionSerial{0};
			bool pending{false}; // a submitted batch will signal inFlight
		};

		// Internals
		void release();
		acm::Error rebuildPresentSemaphores(const acm::vulkan::SwapChain& swapChain);
		static acm::Error recordFrame(acm::CommandBuffer& commandBuffer, const acm::RenderTarget& target, uint32_t frameIndex, const std::function<void(acm::CommandBuffer&, uint32_t)>& prePass, const std::function<void(acm::CommandBuffer&, uint32_t)>& record);
		static bool recordClearFrame(acm::CommandBuffer& commandBuffer, const acm::RenderTarget& target);

		acm::vulkan::Device* m_owner{nullptr};
		acm::SwapChain m_swapChain;
		acm::CommandPool m_commandPool;
		std::vector<acm::CommandBuffer> m_commandBuffers;
		std::vector<Frame> m_frames;
		// One per swapchain image, signalled by the frame's submit and waited by its present.
		// No fence covers a present's wait, but acquiring the same image again does, so these
		// cannot be per frame-in-flight. Rebuilt when the swapchain generation changes.
		std::vector<VkSemaphore> m_presentSemaphores;
		uint64_t m_presentGeneration{0};
		size_t m_currentFrame{0};
		bool m_needsRecreate{false};
		acm::Error m_error;
	};
} // namespace acm::vulkan
