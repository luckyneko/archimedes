/*
 *  Created by LuckyNeko on 07/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include <archimedes/archimedes.h>
#include <vulkan/vulkan.h>

#include <catch2/catch_all.hpp>

// Shared scaffolding for the [gpu] integration tests. Header-only (inline) so
// each test translation unit can include it without an extra link target.
namespace acmtest
{
	// Creates a windowless VkSurfaceKHR via VK_EXT_headless_surface (enabled by
	// the instance's VK_*_surface match), or VK_NULL_HANDLE if the driver lacks
	// it. The caller owns it — typically by handing it to acm::Surface.
	inline VkSurfaceKHR createHeadlessSurface(acm::Instance& instance)
	{
		auto fn = reinterpret_cast<PFN_vkCreateHeadlessSurfaceEXT>(
			vkGetInstanceProcAddr(instance.vulkanInstance(), "vkCreateHeadlessSurfaceEXT"));
		if (!fn)
			return VK_NULL_HANDLE;

		VkHeadlessSurfaceCreateInfoEXT info = {};
		info.sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT;

		VkSurfaceKHR surface = VK_NULL_HANDLE;
		if (fn(instance.vulkanInstance(), &info, nullptr, &surface) != VK_SUCCESS)
			return VK_NULL_HANDLE;
		return surface;
	}

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

		VkSurfaceKHR vkSurface = createHeadlessSurface(out.instance);
		if (vkSurface == VK_NULL_HANDLE)
		{
			SKIP("headless surface unavailable");
			return false;
		}
		out.surface = out.instance.createVulkanSurface(vkSurface);

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
