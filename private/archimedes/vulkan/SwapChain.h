/*
 *  Created by LuckyNeko on 24/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmRenderTarget.h"
#include "archimedes/acmSurface.h"
#include "archimedes/acmTypes.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace acm::vulkan
{
	class Device;
	class Surface;

	// Move-only VkSwapchainKHR owner. Rebuilds per-image public RenderTarget handles and
	// invalidates old target generations on recreate/destruction.
	class SwapChain
	{
	public:
		// Lifetime
		SwapChain() = default;
		SwapChain(acm::vulkan::Device& owner, const acm::Surface& surface, acm::SurfaceFormat format, acm::PresentMode presentMode, const acm::SwapChainConfig& config);
		~SwapChain();
		SwapChain(const SwapChain&) = delete;
		SwapChain& operator=(const SwapChain&) = delete;
		SwapChain(SwapChain&& other) noexcept;
		SwapChain& operator=(SwapChain&& other) noexcept;

		// State
		acm::vulkan::Device& owner() const { return *m_owner; }
		bool valid() const { return m_owner && m_swapChain != VK_NULL_HANDLE && m_error.ok(); }
		acm::Error error() const { return m_error; }

		// Images
		// Waits the device idle, then rebuilds; externally synchronized with this swapchain's
		// renderer, but other renderers on the Device may keep submitting.
		bool recreate();
		acm::SurfaceFormat format() const;
		acm::Extent2D extent() const;
		size_t renderTargetCount() const;
		acm::RenderTarget renderTarget(size_t index) const;
		// Counts successful builds, so a renderer can tell its per-image state is stale. Each
		// build after the first follows a device-idle wait.
		uint64_t generation() const { return m_generation; }
		VkResult acquireNextImage(VkSemaphore semaphore, uint32_t& imageIndex) const;
		VkSwapchainKHR vkSwapChain() const;

	private:
		// Internals
		void release();
		// Replaces the swapchain and its targets. The device must have finished with the old
		// ones: recreate() waits it idle first.
		bool rebuild(acm::vulkan::Device& owner);
		void retireTargets(acm::vulkan::Device& owner);

		acm::vulkan::Device* m_owner{nullptr};
		acm::Surface m_surface;
		acm::SurfaceFormat m_format;
		acm::PresentMode m_presentMode{acm::PresentMode::Fifo};
		acm::SwapChainConfig m_config;
		VkSwapchainKHR m_swapChain{VK_NULL_HANDLE};
		acm::Extent2D m_extent;
		std::vector<acm::RenderTarget> m_renderTargets;
		uint64_t m_generation{0};
		acm::Error m_error;
	};
} // namespace acm::vulkan
