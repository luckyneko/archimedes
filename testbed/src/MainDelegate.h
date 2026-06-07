#pragma once

#include "WindowDelegate.h"
#include <vulkan/vulkan.h>
#include <vector>

// The testbed's main demo: draws a hardcoded triangle. Selects a graphics+
// present capable GPU/queue, builds a pipeline + per-frame sync against the
// swapchain's render targets, and presents.
class MainDelegate : public WindowDelegate
{
	public:
		SwapChainSettings onSelectSwapChainSettings(const std::vector<acm::GPU>& gpus, const std::vector<acm::GPUSurfaceSupport>& surfaceSupport) final;

		void onInit(acm::Device device, acm::SwapChain swapChain) final;
		void onShutdown(acm::Device device, acm::SwapChain swapChain) final;
		void onUpdate() final;
		void onRender(acm::Device device, acm::SwapChain swapChain) final;

	private:
		bool createPipeline(acm::Device device, acm::SwapChain swapChain);
		bool createCommandBuffers(acm::Device device, acm::SwapChain swapChain);
		bool createSyncObjects(acm::Device device);

		struct FrameSync
		{
			VkSemaphore imageAvailable{ VK_NULL_HANDLE };
			VkSemaphore renderFinished{ VK_NULL_HANDLE };
			VkFence inFlight{ VK_NULL_HANDLE };
		};

		bool m_ready{ false };
		VkPipelineLayout m_pipelineLayout{ VK_NULL_HANDLE };
		VkPipeline m_pipeline{ VK_NULL_HANDLE };
		VkCommandPool m_commandPool{ VK_NULL_HANDLE };
		std::vector<VkCommandBuffer> m_commandBuffers;
		std::vector<FrameSync> m_frames;
		size_t m_currentFrame{ 0 };
};
