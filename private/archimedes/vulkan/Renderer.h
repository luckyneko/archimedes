#pragma once

#include "archimedes/acmCommandBuffer.h"
#include "archimedes/acmCommandPool.h"
#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmSwapChain.h"

#include <vulkan/vulkan.h>

#include <cstddef>
#include <functional>
#include <vector>

namespace acm::vulkan
{
	class Device;

	class Renderer
	{
	public:
		bool create(acm::vulkan::Device& owner, const acm::SwapChain& swapChain);
		acm::vulkan::Device& owner() const { return *m_owner; }
		acm::Error render(const std::function<void(acm::CommandBuffer&, uint32_t)>& prePass, const std::function<void(acm::CommandBuffer&, uint32_t)>& record);
		void retire(acm::vulkan::Device& owner);

	private:
		struct Frame
		{
			VkSemaphore imageAvailable{VK_NULL_HANDLE};
			VkSemaphore renderFinished{VK_NULL_HANDLE};
			VkFence inFlight{VK_NULL_HANDLE};
		};

		acm::vulkan::Device* m_owner{nullptr};
		acm::SwapChain m_swapChain;
		acm::CommandPool m_commandPool;
		std::vector<acm::CommandBuffer> m_commandBuffers;
		std::vector<Frame> m_frames;
		size_t m_currentFrame{0};
		bool m_needsRecreate{false};
	};
} // namespace acm::vulkan
