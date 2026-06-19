#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmTypes.h"
#include "archimedes/acmVkFwd.h"

namespace acm
{
	class SwapChain
	{
	public:
		SwapChain() {}

		inline void reset() { m.reset(); m_error = {}; }
		inline bool valid() const { return m != nullptr; }
		acm::Error error() const { return m_error; }

		// Rebuilds the swapchain + render targets at the surface's current size
		// (e.g. after a resize / VK_ERROR_OUT_OF_DATE_KHR), preserving the render
		// pass. Waits for the device to idle first. Returns false when the surface
		// is zero-sized (minimized) — the caller should skip the frame and retry.
		bool recreate();

		VkSwapchainKHR vkSwapChain();
		VkRenderPass vkRenderPass(); // shared by every render target
		acm::SurfaceFormat getFormat() const;
		acm::Extent2D getExtents() const;
		size_t getRenderTargetCount() const;
		acm::RenderTarget getRenderTarget(size_t idx) const;

	private:
		friend class Device; // only Device::createSwapChain builds one
		// desiredExtent is only used when the surface defers sizing to the
		// app (currentExtent == UINT32_MAX, e.g. headless); it is clamped to
		// the surface's min/max. A window-backed surface ignores it. `depth` gives
		// every target a depth buffer (and the shared render pass a depth attachment);
		// `samples` > 1 makes targets multisampled (resolving into the presented image).
		SwapChain(acm::Device device, acm::Surface surface, acm::SurfaceFormat format, acm::PresentMode presentMode, acm::Extent2D desiredExtent = {}, bool depth = false, acm::SampleCount samples = acm::SampleCount::One);

		struct impl;
		std::shared_ptr<impl> m;
		acm::Error m_error;
	};
} // namespace acm
