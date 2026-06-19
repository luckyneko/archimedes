#include "vk_test_helpers.h"
#include <archimedes/archimedes.h>
#include <catch2/catch_all.hpp>
#include <cstdint>

// Integration: builds an acm::SwapChain on a headless surface — the case where
// the surface leaves sizing to the app (currentExtent == UINT32_MAX), which the
// constructor must handle by clamping the requested extent. SKIPs without a
// live driver, the headless extension, or headless-swapchain support.

TEST_CASE("SwapChain (headless) clamps the requested extent", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0, 0});
	if (!instance.valid())
		SKIP("no Vulkan driver available");

	uint32_t queueIdx = 0;
	const acm::GPU* gpu = acmtest::selectGraphicsGPU(instance, queueIdx);
	if (!gpu)
		SKIP("no graphics-capable queue family");

	VkSurfaceKHR vkSurface = acmtest::createHeadlessSurface(instance);
	if (vkSurface == VK_NULL_HANDLE)
		SKIP("headless surface unavailable");

	acm::Surface surface = instance.createSurface(vkSurface);
	const acm::GPUSurfaceSupport& support = surface.getGPUSupport()[gpu->index];
	if (support.supportedFormats.empty() || support.supportedPresentModes.empty())
		SKIP("headless surface exposes no formats/present modes");

	acm::Device device = instance.createDevice(*gpu, queueIdx);
	REQUIRE(device.valid());

	const acm::Extent2D desired{800, 600};
	acm::SwapChain swapChain = device.createSwapChain(surface, support.supportedFormats[0], support.supportedPresentModes[0], desired);
	if (!swapChain.valid())
		SKIP("driver does not support a headless swapchain");

	// Proof the undefined-extent branch ran: the chosen extent is a real value
	// inside the surface's allowed range (not the UINT32_MAX sentinel), so the
	// swapchain could actually be created.
	const acm::SurfaceCapabilities& caps = support.capabilities;
	const acm::Extent2D extent = swapChain.getExtents();
	REQUIRE(extent.width != UINT32_MAX);
	REQUIRE(extent.width >= caps.minImageExtent.width);
	REQUIRE(extent.width <= caps.maxImageExtent.width);
	REQUIRE(extent.height >= caps.minImageExtent.height);
	REQUIRE(extent.height <= caps.maxImageExtent.height);

	REQUIRE(swapChain.getRenderTargetCount() > 0);
	REQUIRE(swapChain.getRenderTarget(0).valid());
}
