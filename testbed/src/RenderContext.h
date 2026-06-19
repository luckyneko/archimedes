#pragma once

#include <archimedes/archimedes.h>

// One window's generic render plumbing: its swapchain + renderer, shared by every example.
// The example-specific content (pipelines, descriptors, scene) lives in the Example; it
// records into this context's renderer. The GLFW window + acm::Surface are owned by the
// App (they must outlive the device), so RenderContext borrows the surface to build the
// swapchain. Same handle/all-or-nothing shape as the acm:: types.
class RenderContext
{
public:
	RenderContext() {}
	bool init(acm::Device device, acm::Surface surface, acm::SurfaceFormat format, acm::PresentMode presentMode, bool depth, acm::SampleCount samples, acm::Extent2D extent);

	acm::Renderer renderer() { return m_renderer; }
	acm::SwapChain swapChain() { return m_swapChain; }
	acm::Extent2D extent() { return m_swapChain.getExtents(); }
	VkRenderPass renderPass() { return m_swapChain.vkRenderPass(); } // for pipeline creation

	void shutdown();

private:
	acm::SwapChain m_swapChain;
	acm::Renderer m_renderer;
};
