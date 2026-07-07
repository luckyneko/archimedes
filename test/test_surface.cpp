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
#include <cstddef>
#include <vector>

// Integration: exercises acm::Surface without a window via the first-class
// acm::Instance::createHeadlessSurface() (VK_EXT_headless_surface where available,
// e.g. MoltenVK, else an off-screen platform window). SKIPs without a live driver
// or any headless mechanism.

TEST_CASE("SurfacePreferences presets expose color and pacing policy", "[acm][unit]")
{
	const acm::SurfacePreferences empty;
	REQUIRE(empty.formats.empty());
	REQUIRE(empty.presentModes.empty());

	const acm::SurfacePreferences defaults = acm::SurfacePreferences::Default();
	REQUIRE(defaults.formats.size() == 2);
	REQUIRE(defaults.formats[0].format == acm::Format::B8G8R8A8_Unorm);
	REQUIRE(defaults.formats[1].format == acm::Format::R8G8B8A8_Unorm);
	REQUIRE(defaults.presentModes.size() == 4);
	REQUIRE(defaults.presentModes[0] == acm::PresentMode::Fifo);

	const acm::SurfacePreferences hardwareSrgb = acm::SurfacePreferences::HardwareSrgb();
	REQUIRE(hardwareSrgb.formats.size() == 2);
	REQUIRE(hardwareSrgb.formats[0].format == acm::Format::B8G8R8A8_Srgb);
	REQUIRE(hardwareSrgb.formats[1].format == acm::Format::R8G8B8A8_Srgb);
	REQUIRE(hardwareSrgb.presentModes[0] == acm::PresentMode::Fifo);

	const acm::SurfacePreferences lowLatency = acm::SurfacePreferences::LowLatency();
	REQUIRE(lowLatency.formats[0].format == acm::Format::B8G8R8A8_Unorm);
	REQUIRE(lowLatency.presentModes.size() == 4);
	REQUIRE(lowLatency.presentModes[0] == acm::PresentMode::Mailbox);
	REQUIRE(lowLatency.presentModes[1] == acm::PresentMode::Immediate);
	REQUIRE(lowLatency.presentModes[2] == acm::PresentMode::FifoRelaxed);
	REQUIRE(lowLatency.presentModes[3] == acm::PresentMode::Fifo);
}

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

	acm::SurfacePreferences preferences;
	preferences.presentModes = {
		acm::PresentMode::Immediate,
		acm::PresentMode::Mailbox,
		acm::PresentMode::FifoRelaxed,
		acm::PresentMode::Fifo,
	};
	const std::vector<acm::SurfaceOption> preferredOptions = instance.surfaceOptions(surface, preferences);
	REQUIRE_FALSE(preferredOptions.empty());
	const auto sameDevice = [](const acm::DeviceOption& a, const acm::DeviceOption& b)
	{
		return a.deviceIndex == b.deviceIndex && a.queueFamily == b.queueFamily;
	};
	const auto sameFormat = [](const acm::SurfaceFormat& a, const acm::SurfaceFormat& b)
	{
		return a.format == b.format && a.colorSpace == b.colorSpace;
	};
	const auto presentModeRank = [&preferences](acm::PresentMode presentMode)
	{
		for (size_t index = 0; index < preferences.presentModes.size(); ++index)
			if (preferences.presentModes[index] == presentMode)
				return index;
		return preferences.presentModes.size();
	};
	const acm::SurfaceOption firstOption = preferredOptions.front();
	size_t previousRank = 0;
	for (const acm::SurfaceOption& option : preferredOptions)
	{
		if (!sameDevice(option.device, firstOption.device) || !sameFormat(option.format, firstOption.format))
			break;
		const size_t rank = presentModeRank(option.presentMode);
		REQUIRE(rank >= previousRank);
		previousRank = rank;
	}

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
