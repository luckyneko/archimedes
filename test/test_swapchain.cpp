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
#include <cstdint>
#include <vector>

// Integration: builds an acm::SwapChain on a headless surface — the case where
// the surface leaves sizing to the app (currentExtent == UINT32_MAX), which the
// constructor must handle by clamping the requested extent. SKIPs without a
// live driver, the headless extension, or headless-swapchain support.

TEST_CASE("SwapChain (headless) clamps the requested extent", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	if (!instance.valid())
		SKIP("no Vulkan driver available");

	acm::Surface surface = instance.createHeadlessSurface(acm::Extent2D{800, 600});
	if (!surface.valid())
		SKIP("headless surface unavailable");

	const std::vector<acm::SurfaceOption> options = instance.surfaceOptions(surface);
	if (options.empty())
		SKIP("headless surface exposes no compatible device options");
	const acm::SurfaceOption option = options.front();
	const acm::SurfaceDeviceSupport& support = surface.deviceSupport()[option.device.deviceIndex];

	acm::Device device = instance.createDevice(option.device);
	REQUIRE(device.valid());

	const acm::Extent2D desired{800, 600};
	acm::SwapChain swapChain = device.createSwapChain(surface, option.format, option.presentMode, desired);
	if (!swapChain.valid())
		SKIP("driver does not support a headless swapchain");

	// Proof the undefined-extent branch ran: the chosen extent is a real value
	// inside the surface's allowed range (not the UINT32_MAX sentinel), so the
	// swapchain could actually be created.
	const acm::SurfaceCapabilities& caps = support.capabilities;
	const acm::Extent2D extent = swapChain.extent();
	REQUIRE(extent.width != UINT32_MAX);
	REQUIRE(extent.width >= caps.minImageExtent.width);
	REQUIRE(extent.width <= caps.maxImageExtent.width);
	REQUIRE(extent.height >= caps.minImageExtent.height);
	REQUIRE(extent.height <= caps.maxImageExtent.height);

	REQUIRE(swapChain.renderTargetCount() > 0);
	acm::RenderTarget target = swapChain.renderTarget(0);
	REQUIRE(target.valid());
	acm::SwapChain retained = swapChain;
	swapChain.reset();
	REQUIRE(retained.valid());
	REQUIRE(target.valid()); // the shared swapchain still holds the images alive
	retained.reset();
	// Releasing the last swapchain reference marks its slot dead but defers the
	// backend destruction (and thus the render-target invalidation) to garbage
	// collection. waitIdle() drives that collection, after which copied target
	// wrappers are stale rather than referring to a freed swapchain image.
	device.waitIdle();
	REQUIRE_FALSE(target.valid());
}
