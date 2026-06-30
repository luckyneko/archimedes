#pragma once

#include "archimedes/acmForward.h"
#include "archimedes/acmRenderTarget.h"
#include "archimedes/acmSurface.h"
#include "archimedes/acmTypes.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace acm::vulkan
{
	class Device;
	class Surface;

	class SwapChain
	{
	public:
		bool create(acm::vulkan::Device& owner, const acm::Surface& surface, acm::SurfaceFormat format, acm::PresentMode presentMode, acm::Extent2D desiredExtent, bool depth, acm::SampleCount samples);
		acm::vulkan::Device& owner() const { return *m_owner; }
		bool recreate();
		acm::SurfaceFormat format() const;
		acm::Extent2D extent() const;
		size_t renderTargetCount() const;
		acm::RenderTarget renderTarget(size_t index) const;
		VkResult acquireNextImage(VkSemaphore semaphore, uint32_t& imageIndex) const;
		VkSwapchainKHR vkSwapChain() const;
		void retire(acm::vulkan::Device& owner);

	private:
		bool rebuild(acm::vulkan::Device& owner);
		void retireTargets(acm::vulkan::Device& owner);

		acm::vulkan::Device* m_owner{nullptr};
		acm::Surface m_surface;
		acm::SurfaceFormat m_format;
		acm::PresentMode m_presentMode{acm::PresentMode::Fifo};
		acm::Extent2D m_desiredExtent;
		bool m_depth{false};
		acm::SampleCount m_samples{acm::SampleCount::One};
		VkSwapchainKHR m_swapChain{VK_NULL_HANDLE};
		acm::Extent2D m_extent;
		std::vector<acm::RenderTarget> m_renderTargets;
	};
} // namespace acm::vulkan
