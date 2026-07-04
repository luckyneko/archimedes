/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "test_spirv.h"
#include "vk_test_helpers.h"

#include <archimedes/archimedes.h>

#include <catch2/catch_all.hpp>
#include <cstdint>
#include <utility>
#include <vector>

// Integration: a compute pipeline dispatched over a storage buffer, read back on the
// CPU. The storage-buffer descriptor (the writable resource compute needs) is the same
// one the graphics tests use. Surface-free — needs only a graphics/compute queue.

TEST_CASE("a compute shader fills a storage buffer", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	if (!instance.valid())
		SKIP("no Vulkan driver available");
	uint32_t queueIndex = 0;
	const acm::DeviceInfo* gpu = acmtest::selectGraphicsDevice(instance, queueIndex);
	if (!gpu)
		SKIP("no graphics-capable queue family");
	acm::Device device = instance.createDevice(*gpu, queueIndex);
	REQUIRE(device.valid());

	// 256 elements = 4 workgroups of local_size_x = 64 (so no bounds check needed).
	constexpr uint32_t kCount = 256;
	acm::Buffer storage = device.createBuffer(kCount * sizeof(uint32_t), acm::BufferUsage::Storage);
	REQUIRE(storage.valid());
	std::vector<uint32_t> zeros(kCount, 0);
	storage.write(zeros.data(), zeros.size() * sizeof(uint32_t));

	acm::DescriptorSetLayout layout = device.createDescriptorSetLayout({
		{0, acm::DescriptorType::StorageBuffer, acm::ShaderStage::Compute},
	});
	REQUIRE(layout.valid());
	acm::DescriptorSet descriptors = device.createDescriptorSet(layout);
	REQUIRE(descriptors.valid());
	descriptors.setBuffer(0, storage);

	acm::Shader compute = device.createShader(acmtest::computeFillSpirv());
	REQUIRE(compute.valid());
	acm::ComputePipeline pipeline = device.createComputePipeline(compute, layout);
	REQUIRE(pipeline.valid());
	acm::ComputePipeline retained = pipeline;
	pipeline.reset();
	REQUIRE_FALSE(pipeline.valid());
	REQUIRE(retained.valid());
	pipeline = std::move(retained);
	REQUIRE(pipeline.valid());
	REQUIRE_FALSE(retained.valid());

	// Dispatch 4 groups; submitSync records + submits + waits the queue idle.
	device.submitSync([&](acm::CommandBuffer& cmd)
					  {
						  cmd.bindComputePipeline(pipeline);
						  cmd.bindComputeDescriptorSet(pipeline, descriptors);
						  cmd.dispatch(kCount / 64, 1, 1); });

	// The shader wrote values[i] = i*2 + 1; the CPU reads them back.
	const auto* values = static_cast<const uint32_t*>(storage.map());
	REQUIRE(values != nullptr);
	bool allCorrect = true;
	for (uint32_t i = 0; i < kCount; ++i)
		if (values[i] != i * 2u + 1u)
			allCorrect = false;
	storage.unmap();
	REQUIRE(allCorrect);
}

TEST_CASE("a barrier feeds compute output into a graphics read in one command buffer", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	if (!instance.valid())
		SKIP("no Vulkan driver available");
	uint32_t queueIndex = 0;
	const acm::DeviceInfo* gpu = acmtest::selectGraphicsDevice(instance, queueIndex);
	if (!gpu)
		SKIP("no graphics-capable queue family");
	acm::Device device = instance.createDevice(*gpu, queueIndex);
	REQUIRE(device.valid());

	constexpr uint32_t kSize = 32;
	const acm::Extent2D extent{kSize, kSize};

	// One storage buffer (a vec4 color), seeded red so we can tell the compute ran.
	acm::Buffer storage = device.createBuffer(sizeof(float) * 4, acm::BufferUsage::Storage);
	REQUIRE(storage.valid());
	const float red[4] = {1.0f, 0.0f, 0.0f, 1.0f};
	storage.write(red, sizeof(red));

	// Binding 0 is visible to both the compute writer and the fragment reader.
	acm::DescriptorSetLayout layout = device.createDescriptorSetLayout({
		{0, acm::DescriptorType::StorageBuffer, acm::ShaderStage::Compute | acm::ShaderStage::Fragment},
	});
	REQUIRE(layout.valid());
	acm::DescriptorSet descriptors = device.createDescriptorSet(layout);
	descriptors.setBuffer(0, storage);

	acm::ComputePipeline compute = device.createComputePipeline(device.createShader(acmtest::computeColorSpirv()), layout);
	REQUIRE(compute.valid());

	acm::Texture color = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::RenderTarget target = device.createRenderTarget(color, acm::RenderTargetFinish::CopySrc);
	acm::PipelineConfig config;
	config.vertex = device.createShader(acmtest::fullscreenVertSpirv());
	config.fragment = device.createShader(acmtest::storageReadFragSpirv());
	config.target = target;
	config.descriptorLayout = layout;
	acm::Pipeline graphics = device.createPipeline(config);
	REQUIRE(graphics.valid());

	acm::Buffer readback = device.createBuffer(size_t(kSize) * kSize * 4, acm::BufferUsage::TransferDst);

	// One command buffer: compute writes the color, a barrier makes it visible to the
	// fragment stage, then the draw reads it — all without a queue round-trip between.
	acm::CommandPool pool = device.createCommandPool();
	acm::CommandBuffer cmd = pool.allocate();
	cmd.begin();
	cmd.bindComputePipeline(compute);
	cmd.bindComputeDescriptorSet(compute, descriptors);
	cmd.dispatch(1);
	cmd.bufferBarrier(storage, acm::ShaderStage::Compute, acm::ShaderStage::Fragment);
	cmd.beginRendering(target);
	cmd.setViewportAndScissor(extent);
	cmd.bindPipeline(graphics);
	cmd.bindDescriptorSet(graphics, descriptors);
	cmd.draw(3);
	cmd.endRendering();
	cmd.copyTextureToBuffer(color, readback);
	cmd.end();

	REQUIRE_FALSE(device.submitSync(cmd));

	// The center pixel is the compute-written green, not the seeded red — proving the
	// fragment read saw the compute write through the barrier.
	const auto* px = static_cast<const uint8_t*>(readback.map());
	const size_t center = (size_t(kSize / 2) * kSize + kSize / 2) * 4;
	const uint8_t b = px[center + 0], g = px[center + 1], r = px[center + 2];
	readback.unmap();
	REQUIRE(g > 200);
	REQUIRE(r < 60);
	REQUIRE(b < 60);
}
