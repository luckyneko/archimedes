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

TEST_CASE("Device defers destruction until the tagged frame retires", "[acm][gpu]")
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

	device.beginFrame(); // currentFrame == 1
	REQUIRE(device.currentFrame() == 1);

	bool destroyed = false;
	device.enqueueDestroy([&destroyed]{ destroyed = true; });

	// Collecting an earlier frame must not run a later-tagged entry.
	device.collectGarbage(0);
	REQUIRE_FALSE(destroyed);

	// Advancing frames alone does not collect.
	device.beginFrame();
	device.beginFrame();
	device.collectGarbage(0);
	REQUIRE_FALSE(destroyed);

	// Collecting at/after the enqueue frame runs it exactly once.
	device.collectGarbage(1);
	REQUIRE(destroyed);

	destroyed = false;
	device.collectGarbage(device.currentFrame());
	REQUIRE_FALSE(destroyed); // already erased — no double free
}

TEST_CASE("Device flushes outstanding destroys on teardown", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0, 0});
	if(!instance.valid())
		SKIP("no Vulkan driver available");

	uint32_t queueIdx = 0;
	const acm::GPU* chosen = acmtest::selectGraphicsGPU(instance, queueIdx);
	if(!chosen)
		SKIP("no graphics-capable queue family");

	bool flushed = false;
	{
		acm::Device device(instance, *chosen, queueIdx);
		REQUIRE(device.valid());
		device.beginFrame();
		device.enqueueDestroy([&flushed]{ flushed = true; });
		// Never collected: the destructor must wait idle and flush the remainder.
	}
	REQUIRE(flushed);
}
