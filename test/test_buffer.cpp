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

// Integration: Buffer's heap-by-usage behavior. A host-visible buffer (Uniform /
// TransferDst) is CPU-mappable; a device-local one (Vertex / Index) is not, so map()
// returns null there and uploads go through staging instead. (That the staged upload
// actually lands on the DeviceInfo is proven by the indexed-draw test, which renders from a
// device-local vertex buffer.) Device-only — runs anywhere with a graphics queue.

TEST_CASE("Buffer picks its heap by usage", "[acm][gpu]")
{
	acm::Instance instance = acmtest::createInstance();
	acmtest::requireInstance(instance);

	uint32_t queueIndex = 0;
	const acm::DeviceInfo* gpu = acmtest::selectGraphicsDevice(instance, queueIndex);

	acm::Device device = instance.createDevice(*gpu, queueIndex);
	REQUIRE(device.valid());

	const uint32_t data[3] = {1, 2, 3};

	// Host-visible: mappable, and a round-trip through map() sees the written bytes.
	acm::Buffer host = device.createBuffer(sizeof(data), acm::BufferUsage::Uniform);
	REQUIRE(host.valid());
	REQUIRE_FALSE(host.write(data, sizeof(data)));
	const auto* mapped = static_cast<const uint32_t*>(host.map());
	REQUIRE(mapped != nullptr);
	REQUIRE(mapped[0] == 1);
	REQUIRE(mapped[2] == 3);
	host.unmap();

	// Buffer copies share the device-owned record without a shared_ptr pImpl.
	acm::Buffer copy = host;
	host.reset();
	REQUIRE_FALSE(host.valid());
	REQUIRE(copy.valid());
	REQUIRE(static_cast<const uint32_t*>(copy.map())[1] == 2);

	// Device-local: not host-visible, so map() refuses; write() still works (staging).
	acm::Buffer local = device.createBuffer(sizeof(data), acm::BufferUsage::Vertex);
	REQUIRE(local.valid());
	REQUIRE(local.map() == nullptr);
	REQUIRE_FALSE(local.write(data, sizeof(data))); // staged upload
	REQUIRE(local.valid());
}

// Buffer::write writes exactly what it is given or nothing: an oversize write and a null
// source are errors (not silently truncated), while an exact-size or smaller write
// succeeds, and a smaller write leaves the rest of the buffer alone. Covers both heaps:
// a host-visible buffer (checked through its mapping) and a device-local one (staged).
TEST_CASE("Buffer::write rejects oversize and null writes", "[acm][gpu]")
{
	acm::Instance instance = acmtest::createInstance();
	acmtest::requireInstance(instance);

	uint32_t queueIndex = 0;
	const acm::DeviceInfo* gpu = acmtest::selectGraphicsDevice(instance, queueIndex);

	acm::Device device = instance.createDevice(*gpu, queueIndex);
	REQUIRE(device.valid());

	const uint32_t exact[3] = {10, 20, 30};
	const uint32_t oversize[4] = {50, 60, 70, 80};
	const uint32_t smaller[1] = {90};

	// Host-visible: the mapping shows nothing of a rejected write lands.
	acm::Buffer host = device.createBuffer(sizeof(exact), acm::BufferUsage::Uniform);
	REQUIRE(host.valid());
	REQUIRE_FALSE(host.write(exact, sizeof(exact)));
	REQUIRE(host.write(oversize, sizeof(oversize)));
	REQUIRE(host.write(nullptr, sizeof(exact)));
	const auto* mapped = static_cast<const uint32_t*>(host.map());
	REQUIRE(mapped != nullptr);
	REQUIRE(mapped[0] == 10);
	REQUIRE(mapped[1] == 20);
	REQUIRE(mapped[2] == 30);
	REQUIRE_FALSE(host.write(smaller, sizeof(smaller)));
	REQUIRE(mapped[0] == 90);
	REQUIRE(mapped[1] == 20);
	REQUIRE(mapped[2] == 30);
	REQUIRE_FALSE(host.write(exact, 0));
	REQUIRE(host.valid());

	// Device-local: the same rules on the staged path.
	acm::Buffer local = device.createBuffer(sizeof(exact), acm::BufferUsage::Vertex);
	REQUIRE(local.valid());
	REQUIRE(local.write(oversize, sizeof(oversize)));
	REQUIRE(local.write(nullptr, sizeof(exact)));
	REQUIRE_FALSE(local.write(exact, sizeof(exact)));
	REQUIRE_FALSE(local.write(smaller, sizeof(smaller)));
	REQUIRE(local.valid());
}
