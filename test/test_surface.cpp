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
#include <vector>

// Integration: exercises acm::Surface without a window via the first-class
// acm::Instance::createHeadlessSurface() (VK_EXT_headless_surface where available,
// e.g. MoltenVK, else an off-screen platform window). SKIPs without a live driver
// or any headless mechanism.

TEST_CASE("Surface (headless) produces compatible surface options", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	if (!instance.valid())
		SKIP("no Vulkan driver available");

	acm::Surface surface = instance.createHeadlessSurface();
	if (!surface.valid())
		SKIP("headless surface unavailable");

	const std::vector<acm::SurfaceOption> options = instance.surfaceOptions(surface);
	if (options.empty())
		SKIP("headless surface exposes no compatible device options");
	const std::vector<acm::DeviceInfo>& devices = instance.devices();
	for (const acm::SurfaceOption& option : options)
	{
		REQUIRE(option.device.deviceIndex < devices.size());
		const acm::DeviceInfo& device = devices[option.device.deviceIndex];
		REQUIRE(option.device.queueFamily < device.queues.size());
		REQUIRE(device.queues[option.device.queueFamily].graphics);
		REQUIRE(option.capabilities.minImageCount > 0);
		REQUIRE(option.format.format != acm::Format::Undefined);
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
