#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmTypes.h"
#include "archimedes/acmVkFwd.h"

namespace acm
{
	class RenderTarget
	{
	public:
		RenderTarget() {}

		inline void reset() { m.reset(); m_error = {}; }
		inline bool valid() const { return m != nullptr; }
		acm::Error error() const { return m_error; }

		acm::Extent2D getExtent() const;
		// Whether this target has a depth attachment (so callers / the command buffer
		// know to supply a depth clear value and use a depth-testing pipeline).
		bool hasDepth() const;
		// Whether this target is multisampled (samples > 1) — it then owns an MSAA color
		// (+ depth) image and resolves into the swapchain image / texture. Shifts where
		// the depth clear value sits (a resolve attachment precedes it).
		bool isMultisampled() const;
		VkRenderPass vkRenderPass();
		VkFramebuffer vkFramebuffer();

	private:
		friend class Device; // only Device::createRenderTarget builds one

		// Swapchain target: the render pass is owned by the SwapChain and shared
		// across all of its targets, so it is borrowed here (not destroyed). The
		// image is the swapchain's too — this only creates the view + framebuffer.
		// `depth` adds an owned per-image depth buffer; `samples` > 1 adds an owned MSAA
		// color (+ depth) image that resolves into the swapchain image (the shared pass
		// must match both).
		RenderTarget(acm::Device device, VkRenderPass renderPass, VkImage image, acm::Format format, acm::Extent2D extent, bool depth, acm::SampleCount samples);

		// Offscreen target over an owned Texture: there is no swapchain, so this
		// creates (and owns) a render pass + framebuffer over the texture's view.
		// `finish` sets the (resolve) color attachment's final layout (sampled vs copy
		// src). `depth` adds an owned depth buffer; `samples` > 1 renders multisampled
		// and resolves into the texture. Owned images are kept alive for the target.
		RenderTarget(acm::Device device, acm::Texture texture, acm::RenderTargetFinish finish, bool depth, acm::SampleCount samples);

		struct impl;
		std::shared_ptr<impl> m;
		acm::Error m_error;
	};
} // namespace acm
