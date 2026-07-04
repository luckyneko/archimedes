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

// Shared scaffolding for the [gpu] integration tests. Header-only (inline) so
// each test translation unit can include it without an extra link target. The
// windowless surface (and its Windows off-screen-window fallback) is a first-class
// acm::Instance::createHeadlessSurface() capability, so this stays backend-neutral
// — no raw Vulkan or platform headers.
namespace acmtest
{
	// First graphics-capable GPU and its queue family index; nullptr if none.
	// The pointer is valid for the lifetime of the instance.
	inline const acm::GPU* selectGraphicsGPU(const acm::Instance& instance, uint32_t& queueIndex)
	{
		for (const auto& gpu : instance.getAvailableGPUs())
			for (const auto& qf : gpu.queueFamilies)
				if (qf.supportsGraphics)
				{
					queueIndex = qf.index;
					return &gpu;
				}
		return nullptr;
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

		uint32_t queueIndex = 0;
		const acm::GPU* gpu = selectGraphicsGPU(out.instance, queueIndex);
		if (!gpu)
		{
			SKIP("no graphics-capable queue family");
			return false;
		}

		out.surface = out.instance.createHeadlessSurface(acm::Extent2D{800, 600});
		if (!out.surface.valid())
		{
			SKIP("headless surface unavailable");
			return false;
		}

		const acm::GPUSurfaceSupport& support = out.surface.getGPUSupport()[gpu->index];
		if (support.supportedFormats.empty() || support.supportedPresentModes.empty())
		{
			SKIP("headless surface exposes no formats/present modes");
			return false;
		}

		out.device = out.instance.createDevice(*gpu, queueIndex);
		REQUIRE(out.device.valid());

		out.swapChain = out.device.createSwapChain(out.surface, support.supportedFormats[0], support.supportedPresentModes[0], acm::Extent2D{800, 600});
		if (!out.swapChain.valid())
		{
			SKIP("driver does not support a headless swapchain");
			return false;
		}
		return true;
	}
} // namespace acmtest
