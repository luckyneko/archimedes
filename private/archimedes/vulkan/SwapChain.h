#pragma once

#include "archimedes/acmForward.h"
#include "archimedes/acmRenderTarget.h"
#include "archimedes/acmTypes.h"
#include "archimedes/HandleMap.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace acm::vulkan
{
	class Device;
	class Surface;

	class SwapChain : public acm::ResourceSlot<acm::vulkan::SwapChain, acm::vulkan::Device>
	{
	public:
		bool create(acm::vulkan::Device& owner, acm::vulkan::Surface& surface, const acm::Handle& surfaceHandle, acm::SurfaceFormat format, acm::PresentMode presentMode, acm::Extent2D desiredExtent, bool depth, acm::SampleCount samples);
		bool recreate(const acm::Handle& handle);
		acm::SurfaceFormat format(const acm::Handle& handle) const;
		acm::Extent2D extent(const acm::Handle& handle) const;
		size_t renderTargetCount(const acm::Handle& handle) const;
		acm::RenderTarget renderTarget(const acm::Handle& handle, size_t index) const;
		VkResult acquireNextImage(const acm::Handle& handle, VkSemaphore semaphore, uint32_t& imageIndex) const;
		VkSwapchainKHR vkSwapChain(const acm::Handle& handle) const;
		void retire(acm::vulkan::Device& owner);

	private:
		bool rebuild(acm::vulkan::Device& owner);
		VkRenderPass createRenderPass(acm::vulkan::Device& owner) const;
		void retireTargets(acm::vulkan::Device& owner);

		acm::vulkan::Surface* m_surfaceResource{nullptr};
		acm::Handle m_surface;
		acm::SurfaceFormat m_format;
		acm::PresentMode m_presentMode{acm::PresentMode::Fifo};
		acm::Extent2D m_desiredExtent;
		bool m_depth{false};
		acm::SampleCount m_samples{acm::SampleCount::One};
		VkSwapchainKHR m_swapChain{VK_NULL_HANDLE};
		VkRenderPass m_renderPass{VK_NULL_HANDLE};
		acm::Extent2D m_extent;
		std::vector<acm::RenderTarget> m_renderTargets;
	};
} // namespace acm::vulkan
