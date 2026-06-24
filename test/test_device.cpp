#include "vk_test_helpers.h"

#include <archimedes/archimedes.h>

#include <catch2/catch_all.hpp>

// Integration: creates a logical device on the first graphics-capable queue.
// Headless - no surface/window needed. SKIPs without a live driver.

TEST_CASE("Device creates on a graphics queue", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0, 0});
	if (!instance.valid())
		SKIP("no Vulkan driver available");

	uint32_t queueIdx = 0;
	const acm::GPU* chosen = acmtest::selectGraphicsGPU(instance, queueIdx);
	if (!chosen)
		SKIP("no graphics-capable queue family");
	const uint32_t gpuIndex = chosen->index;

	acm::Device device = instance.createDevice(*chosen, queueIdx);
	REQUIRE(device.valid());
	REQUIRE(device.getQueueIdx() == queueIdx);
	REQUIRE(device.getGPU().index == gpuIndex);
}
