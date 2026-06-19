#include "test_spirv.h"
#include "vk_test_helpers.h"
#include <archimedes/archimedes.h>
#include <catch2/catch_all.hpp>
#include <cstdint>

// Integration: a compute shader writes a storage image, which is then copied back and
// checked. Exercises the StorageImage descriptor, the texture `storage` usage, and the
// image-layout transitions (Undefined → General to write, General → TransferSrc to copy).
// Surface-free, runs anywhere with a graphics/compute queue.

TEST_CASE("a compute shader writes a storage image", "[acm][gpu]")
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

	constexpr uint32_t kSize = 32; // 4x4 workgroups of 8x8
	const acm::Extent2D extent{kSize, kSize};

	// R8G8B8A8_Unorm is a mandated storage-image format. Created with `storage` usage.
	acm::Texture image = device.createTexture(acm::Format::R8G8B8A8_Unorm, extent, /*mipmapped*/ false, /*storage*/ true);
	REQUIRE(image.valid());

	acm::DescriptorSetLayout layout = device.createDescriptorSetLayout({
		{0, acm::DescriptorType::StorageImage, acm::ShaderStage::Compute},
	});
	REQUIRE(layout.valid());
	acm::DescriptorSet descriptors = device.createDescriptorSet(layout);
	REQUIRE(descriptors.valid());
	descriptors.setStorageImage(0, image);

	acm::ComputePipeline pipeline = device.createComputePipeline(device.createShader(acmtest::computeStoreImageSpirv()), layout);
	REQUIRE(pipeline.valid());

	acm::Buffer readback = device.createBuffer(size_t(kSize) * kSize * 4, acm::BufferUsage::TransferDst);
	REQUIRE(readback.valid());

	device.submitSync([&](acm::CommandBuffer cmd)
					  {
						  cmd.transitionImage(image, acm::ImageLayout::Undefined, acm::ImageLayout::General);
						  cmd.bindComputePipeline(pipeline);
						  cmd.bindComputeDescriptorSet(pipeline, descriptors);
						  cmd.dispatch(kSize / 8, kSize / 8, 1);
						  cmd.transitionImage(image, acm::ImageLayout::General, acm::ImageLayout::TransferSrc);
						  cmd.copyTextureToBuffer(image, readback); });

	// The compute shader wrote solid green. R8G8B8A8 memory order is [R,G,B,A].
	const auto* px = static_cast<const uint8_t*>(readback.map());
	const size_t center = (size_t(kSize / 2) * kSize + kSize / 2) * 4;
	const uint8_t r = px[center + 0], g = px[center + 1], b = px[center + 2];
	readback.unmap();
	REQUIRE(g > 200);
	REQUIRE(r < 60);
	REQUIRE(b < 60);
}
