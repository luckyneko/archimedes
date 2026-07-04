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

// Integration: exercises acm::Surface without a window via the first-class
// acm::Instance::createHeadlessSurface() (VK_EXT_headless_surface where available,
// e.g. MoltenVK, else an off-screen platform window). SKIPs without a live driver
// or any headless mechanism.

TEST_CASE("Surface (headless) reports per-GPU support", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	if (!instance.valid())
		SKIP("no Vulkan driver available");

	acm::Surface surface = instance.createHeadlessSurface();
	if (!surface.valid())
		SKIP("headless surface unavailable");

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

	acm::Surface a = instance.createHeadlessSurface();
	if (!a.valid())
		SKIP("headless surface unavailable");

	acm::Surface b = a; // shares the one underlying VkSurfaceKHR
	a.reset();

	REQUIRE_FALSE(a.valid());
	REQUIRE(b.valid());
}
