#include "vk_test_helpers.h"
#include <archimedes/archimedes.h>
#include <catch2/catch_all.hpp>
#include <cstdint>
#include <vector>

// Integration: the pooling memory sub-allocator. Many small buffers should share a few
// large VkDeviceMemory blocks rather than each doing its own vkAllocateMemory — the
// whole point of the pool (staying under maxMemoryAllocationCount, avoiding per-resource
// allocation overhead). We create 200 buffers and assert the device holds only a
// handful of blocks. Device-only — runs anywhere with a graphics queue.

TEST_CASE("buffers share pooled memory blocks", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0, 0});
	if (!instance.valid())
		SKIP("no Vulkan driver available");

	uint32_t queueIdx = 0;
	const acm::GPU* gpu = acmtest::selectGraphicsGPU(instance, queueIdx);
	if (!gpu)
		SKIP("no graphics-capable queue family");

	acm::Device device = instance.createDevice(*gpu, queueIdx);
	REQUIRE(device.valid());

	// Nothing has allocated device memory yet.
	REQUIRE(device.memoryBlockCount() == 0);

	constexpr int kCount = 200;
	std::vector<acm::Buffer> buffers;
	buffers.reserve(kCount);
	for (int i = 0; i < kCount; ++i)
	{
		acm::Buffer b = device.createBuffer(1024, acm::BufferUsage::Vertex);
		REQUIRE(b.valid());
		buffers.push_back(b);
	}

	// 200 KB of buffers fits comfortably in one (or a couple of) pooled blocks — not
	// 200 separate allocations.
	REQUIRE(device.memoryBlockCount() >= 1);
	REQUIRE(device.memoryBlockCount() <= 2);
}
