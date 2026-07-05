/*
 *  Created by LuckyNeko on 07/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include <archimedes/archimedes.h>
#include <vulkan/vulkan.h>

#include <catch2/catch_all.hpp>

// Integration: needs a live Vulkan driver (vendored MoltenVK ICD via
// VK_ICD_FILENAMES). SKIPs when no driver is available.

TEST_CASE("InstanceConfig has production-safe defaults", "[acm][unit]")
{
	constexpr acm::InstanceConfig config;
	STATIC_REQUIRE(config.portability);
	STATIC_REQUIRE_FALSE(config.validation);
	STATIC_REQUIRE_FALSE(config.debug);
}

TEST_CASE("Instance creates and enumerates GPUs", "[acm][gpu]")
{
	acm::InstanceConfig config;
	config.validation = true;
	config.debug = true;
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0}, config);
	if (!instance.valid())
		SKIP("no Vulkan driver available");

	REQUIRE(instance.vulkanInstance() != VK_NULL_HANDLE);
#if defined(__APPLE__)
	REQUIRE(vkGetInstanceProcAddr(instance.vulkanInstance(), "vkCreateMetalSurfaceEXT") != nullptr);
	REQUIRE(vkGetInstanceProcAddr(instance.vulkanInstance(), "vkCreateMacOSSurfaceMVK") == nullptr);
#endif

	const auto& devices = instance.devices();
	REQUIRE_FALSE(devices.empty());

	bool anyGraphics = false;
	for (uint32_t i = 0; i < devices.size(); ++i)
	{
		const auto& device = devices[i];
		REQUIRE(device.index == i);			// index mirrors enumeration order
		REQUIRE_FALSE(device.name.empty()); // neutral device info populated
		REQUIRE((device.apiVersion.major > 1 || (device.apiVersion.major == 1 && device.apiVersion.minor >= 3)));
		REQUIRE_FALSE(device.queues.empty());
		for (const auto& qf : device.queues)
			anyGraphics = anyGraphics || qf.graphics;
	}
	REQUIRE(anyGraphics);
}

TEST_CASE("Instance transfers ownership on move", "[acm][gpu]")
{
	acm::Instance a("acm-tests", acm::Version{0, 1, 0});
	if (!a.valid())
		SKIP("no Vulkan driver available");

	acm::Instance b = std::move(a);
	VkInstance raw = b.vulkanInstance();

	REQUIRE_FALSE(a.valid());
	REQUIRE(b.valid());
	REQUIRE(b.vulkanInstance() == raw);
}
