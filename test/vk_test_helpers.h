/*
 *  Created by LuckyNeko on 07/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include <archimedes/archimedes.h>

#include <catch2/catch_all.hpp>
#include <vector>

// Shared scaffolding for the [gpu] integration tests. Header-only (inline) so
// each test translation unit can include it without an extra link target. The
// windowless surface (and its Windows off-screen-window fallback) is a first-class
// acm::Instance::createHeadlessSurface() capability, so this stays backend-neutral
// — no raw Vulkan or platform headers.
namespace acmtest
{
	// First graphics-capable device and its queue family index; nullptr if none.
	// The pointer is valid for the lifetime of the instance.
	inline const acm::DeviceInfo* selectGraphicsDevice(const acm::Instance& instance, uint32_t& queueIndex)
	{
		const std::vector<acm::DeviceOption> options = instance.graphicsOptions();
		if (options.empty())
			return nullptr;
		const acm::DeviceOption& option = options.front();
		queueIndex = option.queueFamily;
		return &instance.devices()[option.deviceIndex];
	}

	// The full headless stack (instance -> surface -> device -> swapchain) that
	// the pipeline/render-loop tests build on. On any missing layer it SKIPs (with
	// a specific reason) and returns false, so a caller's `if (!build...) return;`
	// leaves a GPU-less run green.
	struct HeadlessStack
	{
		acm::Instance instance;
		acm::Surface surface;
		acm::Device device;
		acm::SwapChain swapChain;
	};

	inline bool buildHeadlessStack(HeadlessStack& out)
	{
		out.instance = acm::Instance("acm-tests", acm::Version{0, 1, 0});
		if (!out.instance.valid())
		{
			SKIP("no Vulkan driver available");
			return false;
		}

		out.surface = out.instance.createHeadlessSurface(acm::Extent2D{800, 600});
		if (!out.surface.valid())
		{
			SKIP("headless surface unavailable");
			return false;
		}

		const std::vector<acm::SurfaceOption> options = out.instance.surfaceOptions(out.surface);
		if (options.empty())
		{
			SKIP("headless surface exposes no compatible device options");
			return false;
		}
		const acm::SurfaceOption option = options.front();

		out.device = out.instance.createDevice(option.device);
		REQUIRE(out.device.valid());

		out.swapChain = out.device.createSwapChain(out.surface, option, acm::SwapChainConfig{acm::Extent2D{800, 600}});
		if (!out.swapChain.valid())
		{
			SKIP("driver does not support a headless swapchain");
			return false;
		}
		return true;
	}
} // namespace acmtest
