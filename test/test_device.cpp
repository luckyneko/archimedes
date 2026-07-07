/*
 *  Created by LuckyNeko on 07/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "vk_test_helpers.h"

#include <archimedes/acmVulkanInterop.h>
#include <archimedes/archimedes.h>

#include <catch2/catch_all.hpp>

// Integration: creates a logical device on the first graphics-capable queue.
// Headless - no surface/window needed. SKIPs without a live driver.

TEST_CASE("Device creates on a graphics queue", "[acm][gpu]")
{
	acm::InstanceConfig config;
	config.validation = true;
	config.debug = true;
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0}, config);
	if (!instance.valid())
		SKIP("no Vulkan driver available");

	const std::vector<acm::DeviceOption> options = instance.graphicsOptions();
	if (options.empty())
		SKIP("no graphics-capable queue family");
	const acm::DeviceOption option = options.front();
	const acm::DeviceInfo& chosen = instance.devices()[option.deviceIndex];

	acm::Device device = instance.createDevice(option);
	REQUIRE(device.valid());
	REQUIRE(device.queueFamily() == option.queueFamily);
	REQUIRE(device.deviceInfo().index == option.deviceIndex);
	REQUIRE(device.enabledFeatures().fillModeNonSolid == chosen.features.fillModeNonSolid);
	REQUIRE(device.enabledFeatures().wideLines == chosen.features.wideLines);
	REQUIRE(device.enabledFeatures().samplerAnisotropy == chosen.features.samplerAnisotropy);
	REQUIRE(device.enabledFeatures().sampleRateShading == chosen.features.sampleRateShading);

	VkQueue queue = acm::interop::queue(device);
	REQUIRE(queue != VK_NULL_HANDLE);
	VkQueue lockedQueue = VK_NULL_HANDLE;
	REQUIRE_FALSE(acm::interop::withQueue(device, [&](VkQueue queueUnderLock)
										  { lockedQueue = queueUnderLock; }));
	REQUIRE(lockedQueue == queue);
}
