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
#include <utility>
#include <vector>

// Integration: creates a logical device on the first graphics-capable queue.
// Headless - no surface/window needed. SKIPs without a live driver.

TEST_CASE("Device creates on a graphics queue", "[acm][gpu]")
{
	acm::InstanceConfig config;
	config.validation = true;
	config.debug = true;
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0}, config);
	acmtest::requireInstance(instance);

	const acm::DeviceOption option = acmtest::selectGraphicsOption(instance);
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

TEST_CASE("Device feature config controls optional and required features", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	acmtest::requireInstance(instance);

	const acm::DeviceOption option = acmtest::selectGraphicsOption(instance);
	const acm::DeviceInfo& chosen = instance.devices()[option.deviceIndex];

	acm::DeviceConfig noOptional;
	noOptional.optionalFeatures = {};
	acm::Device device = instance.createDevice(option, noOptional);
	REQUIRE(device.valid());
	REQUIRE_FALSE(device.enabledFeatures().fillModeNonSolid);
	REQUIRE_FALSE(device.enabledFeatures().wideLines);
	REQUIRE_FALSE(device.enabledFeatures().samplerAnisotropy);
	REQUIRE_FALSE(device.enabledFeatures().sampleRateShading);

	acm::DeviceConfig requiredOnly;
	requiredOnly.optionalFeatures = {};
	const char* requiredName = nullptr;
	if (chosen.features.fillModeNonSolid)
	{
		requiredOnly.requiredFeatures.fillModeNonSolid = true;
		requiredName = "fillModeNonSolid";
	}
	else if (chosen.features.wideLines)
	{
		requiredOnly.requiredFeatures.wideLines = true;
		requiredName = "wideLines";
	}
	else if (chosen.features.samplerAnisotropy)
	{
		requiredOnly.requiredFeatures.samplerAnisotropy = true;
		requiredName = "samplerAnisotropy";
	}
	else if (chosen.features.sampleRateShading)
	{
		requiredOnly.requiredFeatures.sampleRateShading = true;
		requiredName = "sampleRateShading";
	}
	if (!requiredName)
		SKIP("device supports none of the curated optional features");

	acm::Device requiredDevice = instance.createDevice(option, requiredOnly);
	REQUIRE(device.valid());
	REQUIRE(requiredDevice.valid());
	if (requiredOnly.requiredFeatures.fillModeNonSolid)
		REQUIRE(requiredDevice.enabledFeatures().fillModeNonSolid);
	if (requiredOnly.requiredFeatures.wideLines)
		REQUIRE(requiredDevice.enabledFeatures().wideLines);
	if (requiredOnly.requiredFeatures.samplerAnisotropy)
		REQUIRE(requiredDevice.enabledFeatures().samplerAnisotropy);
	if (requiredOnly.requiredFeatures.sampleRateShading)
		REQUIRE(requiredDevice.enabledFeatures().sampleRateShading);
}

TEST_CASE("Device creation fails when a required feature is unavailable", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	acmtest::requireInstance(instance);

	const acm::DeviceOption option = acmtest::selectGraphicsOption(instance);
	const acm::DeviceInfo& chosen = instance.devices()[option.deviceIndex];

	acm::DeviceConfig config;
	const char* missingName = nullptr;
	if (!chosen.features.fillModeNonSolid)
	{
		config.requiredFeatures.fillModeNonSolid = true;
		missingName = "fillModeNonSolid";
	}
	else if (!chosen.features.wideLines)
	{
		config.requiredFeatures.wideLines = true;
		missingName = "wideLines";
	}
	else if (!chosen.features.samplerAnisotropy)
	{
		config.requiredFeatures.samplerAnisotropy = true;
		missingName = "samplerAnisotropy";
	}
	else if (!chosen.features.sampleRateShading)
	{
		config.requiredFeatures.sampleRateShading = true;
		missingName = "sampleRateShading";
	}
	if (!missingName)
		SKIP("device supports all curated optional features");

	acm::Device device = instance.createDevice(option, config);
	REQUIRE_FALSE(device.valid());
	REQUIRE_THAT(device.error().message(), Catch::Matchers::ContainsSubstring(missingName));
}

namespace
{
	// Every Device call on a device with no backend answers a neutral value or an error,
	// and builds only invalid children.
	void requireInertDevice(acm::Device& device)
	{
		REQUIRE_FALSE(device.valid());
		REQUIRE(device.backend() == nullptr);

		REQUIRE(device.deviceInfo().name.empty());
		REQUIRE(device.deviceInfo().queues.empty());
		REQUIRE(device.queueFamily() == 0);
		const acm::DeviceFeatures& features = device.enabledFeatures();
		REQUIRE_FALSE(features.fillModeNonSolid);
		REQUIRE_FALSE(features.wideLines);
		REQUIRE_FALSE(features.samplerAnisotropy);
		REQUIRE_FALSE(features.sampleRateShading);
		REQUIRE(device.maxSampleCount() == acm::SampleCount::One);
		REQUIRE(device.minUniformBufferOffsetAlignment() == 1);
		REQUIRE(device.memoryBlockCount() == 0);
		device.waitIdle();

		bool recorded = false;
		REQUIRE(device.submitSync([&](acm::CommandBuffer&)
								  { recorded = true; }));
		REQUIRE_FALSE(recorded);
		REQUIRE(device.submitSync(acm::CommandBuffer{}));

		REQUIRE_FALSE(device.createCommandPool().valid());
		REQUIRE_FALSE(device.createBuffer(16, acm::BufferUsage::Uniform).valid());
		REQUIRE_FALSE(device.createTexture(acm::Format::B8G8R8A8_Unorm, acm::Extent2D{4, 4}).valid());
		REQUIRE_FALSE(device.createSampler().valid());
		REQUIRE_FALSE(device.createShader(std::vector<char>(4, 0)).valid());
		REQUIRE_FALSE(device.createDescriptorSetLayout(1).valid());
		REQUIRE_FALSE(device.createDescriptorSet(acm::DescriptorSetLayout{}).valid());
		REQUIRE_FALSE(device.createRenderTarget(acm::Texture{}).valid());
		REQUIRE_FALSE(device.createSwapChain(acm::Surface{}, acm::SurfaceOption{}).valid());
		REQUIRE_FALSE(device.createRenderer(acm::SwapChain{}).valid());
	}
} // namespace

TEST_CASE("a default-constructed Device is safe to call", "[acm][handle]")
{
	acm::Device device;
	requireInertDevice(device);
}

TEST_CASE("a reset or moved-from Device is safe to call", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	acmtest::requireInstance(instance);
	const acm::DeviceOption option = acmtest::selectGraphicsOption(instance);

	acm::Device device = instance.createDevice(option);
	REQUIRE(device.valid());
	acm::Device moved = std::move(device);
	REQUIRE(moved.valid());
	requireInertDevice(device);

	moved.reset();
	requireInertDevice(moved);
}
