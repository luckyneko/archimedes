/*
 *  Created by LuckyNeko on 07/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "vk_test_helpers.h"

#include <archimedes/archimedes.h>

#include <catch2/catch_all.hpp>

// Integration: exercises acm::Surface without a window by creating a headless
// VkSurfaceKHR (VK_EXT_headless_surface, enabled by the instance's VK_*_surface
// extension match and supported by MoltenVK). SKIPs without a live driver or
// the extension. acm::Surface takes ownership of the VkSurfaceKHR.

TEST_CASE("Surface (headless) reports per-GPU support", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	if (!instance.valid())
		SKIP("no Vulkan driver available");

	VkSurfaceKHR vkSurface = acmtest::createHeadlessSurface(instance);
	if (vkSurface == VK_NULL_HANDLE)
		SKIP("headless surface unavailable");

	acm::Surface surface = instance.createVulkanSurface(vkSurface); // takes ownership of vkSurface
	REQUIRE(surface.valid());

	// One support entry per enumerated GPU, mirroring its queue families.
	const auto& gpus = instance.getAvailableGPUs();
	const auto& support = surface.getGPUSupport();
	REQUIRE(support.size() == gpus.size());
	for (size_t i = 0; i < support.size(); ++i)
	{
		REQUIRE(support[i].gpuIndex == gpus[i].index);
		REQUIRE(support[i].queueFamilySupportsPresent.size() == gpus[i].queueFamilies.size());
	}
}

TEST_CASE("Surface is a shared handle", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	if (!instance.valid())
		SKIP("no Vulkan driver available");

	VkSurfaceKHR vkSurface = acmtest::createHeadlessSurface(instance);
	if (vkSurface == VK_NULL_HANDLE)
		SKIP("headless surface unavailable");

	acm::Surface a = instance.createVulkanSurface(vkSurface);
	acm::Surface b = a; // shares the one underlying VkSurfaceKHR
	a.reset();

	REQUIRE_FALSE(a.valid());
	REQUIRE(b.valid());
}
