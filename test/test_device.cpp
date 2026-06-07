#include <catch2/catch_all.hpp>
#include <archimedes/archimedes.h>
#include "vk_test_helpers.h"

// Integration: creates a logical device on the first graphics-capable queue.
// Headless — no surface/window needed. SKIPs without a live driver.

TEST_CASE("Device creates on a graphics queue", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0, 0});
	if(!instance.valid())
		SKIP("no Vulkan driver available");

	uint32_t queueIdx = 0;
	const acm::GPU* chosen = acmtest::selectGraphicsGPU(instance, queueIdx);
	if(!chosen)
		SKIP("no graphics-capable queue family");

	acm::Device device(instance, *chosen, queueIdx);
	REQUIRE(device.valid());
	REQUIRE(device.vkDevice() != VK_NULL_HANDLE);
	REQUIRE(device.vkQueue() != VK_NULL_HANDLE);
	REQUIRE(device.getQueueIdx() == queueIdx);
	REQUIRE(device.getGPU().index == chosen->index);
}
