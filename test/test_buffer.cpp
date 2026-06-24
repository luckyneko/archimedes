#include "vk_test_helpers.h"
#include <archimedes/archimedes.h>
#include <catch2/catch_all.hpp>
#include <cstdint>

// Integration: Buffer's heap-by-usage behavior. A host-visible buffer (Uniform /
// TransferDst) is CPU-mappable; a device-local one (Vertex / Index) is not, so map()
// returns null there and uploads go through staging instead. (That the staged upload
// actually lands on the GPU is proven by the indexed-draw test, which renders from a
// device-local vertex buffer.) Device-only — runs anywhere with a graphics queue.

TEST_CASE("Buffer picks its heap by usage", "[acm][gpu]")
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

	const uint32_t data[3] = {1, 2, 3};

	// Host-visible: mappable, and a round-trip through map() sees the written bytes.
	acm::Buffer host = device.createBuffer(sizeof(data), acm::BufferUsage::Uniform);
	REQUIRE(host.valid());
	host.write(data, sizeof(data));
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
	local.write(data, sizeof(data)); // staged upload — must not crash
	REQUIRE(local.valid());
}
