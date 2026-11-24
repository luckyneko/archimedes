/*
 *  Created by LuckyNeko on 19/06/2026.
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

// Integration: the pooling memory sub-allocator. Many small buffers should share a few
// large VkDeviceMemory blocks rather than each doing its own vkAllocateMemory — the
// whole point of the pool (staying under maxMemoryAllocationCount, avoiding per-resource
// allocation overhead). We create 200 buffers and assert the device holds only a
// handful of blocks. Device-only — runs anywhere with a graphics queue.

TEST_CASE("buffers share pooled memory blocks", "[acm][gpu]")
{
	acm::Instance instance = acmtest::createInstance();
	acmtest::requireInstance(instance);

	uint32_t queueIndex = 0;
	const acm::DeviceInfo* gpu = acmtest::selectGraphicsDevice(instance, queueIndex);

	acm::Device device = instance.createDevice(*gpu, queueIndex);
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

// Vulkan forbids a buffer (linear) and an optimal-tiling image (non-linear) from sharing
// a bufferImageGranularity page, so the allocator keeps the two in separate blocks. A
// small texture would fit in the block a small vertex buffer opened, so a second block
// appearing is the observable proof it went elsewhere; further buffers and textures then
// pool into their own kind's block rather than opening more.
TEST_CASE("buffers and textures never share a memory block", "[acm][gpu]")
{
	acm::Instance instance = acmtest::createInstance();
	acmtest::requireInstance(instance);

	uint32_t queueIndex = 0;
	const acm::DeviceInfo* gpu = acmtest::selectGraphicsDevice(instance, queueIndex);

	acm::Device device = instance.createDevice(*gpu, queueIndex);
	REQUIRE(device.valid());
	REQUIRE(device.memoryBlockCount() == 0);

	const acm::Extent2D extent{16, 16};

	acm::Buffer vertices = device.createBuffer(1024, acm::BufferUsage::Vertex);
	REQUIRE(vertices.valid());
	REQUIRE(device.memoryBlockCount() == 1);

	acm::Texture texture = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	REQUIRE(texture.valid());
	REQUIRE(device.memoryBlockCount() == 2);

	acm::Buffer moreVertices = device.createBuffer(1024, acm::BufferUsage::Vertex);
	acm::Texture anotherTexture = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	REQUIRE(moreVertices.valid());
	REQUIRE(anotherTexture.valid());
	REQUIRE(device.memoryBlockCount() == 2);
}
