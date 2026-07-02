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
		acm::Error render(const std::function<void(acm::CommandBuffer&, uint32_t)>& prePass, const std::function<void(acm::CommandBuffer&, uint32_t)>& record);

	private:
		// Internals
		void release();

		struct Frame
		{
			VkSemaphore imageAvailable{VK_NULL_HANDLE};
			VkSemaphore renderFinished{VK_NULL_HANDLE};
			VkFence inFlight{VK_NULL_HANDLE};
			uint64_t submissionSerial{0};
		};

		acm::vulkan::Device* m_owner{nullptr};
		acm::SwapChain m_swapChain;
		acm::CommandPool m_commandPool;
		std::vector<acm::CommandBuffer> m_commandBuffers;
		std::vector<Frame> m_frames;
		size_t m_currentFrame{0};
		bool m_needsRecreate{false};
		acm::Error m_error;
	};
} // namespace acm::vulkan
