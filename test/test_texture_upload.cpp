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
#include <vector>

// Integration: Texture::upload. CPU pixels (solid green) are staged into texture A,
// which is then sampled into texture B and read back. Green appears nowhere in the
// sample path's shaders, so a green center proves the uploaded pixels were what got
// sampled. Surface-free, runs anywhere with a graphics queue.

TEST_CASE("uploaded texture pixels can be sampled", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	if (!instance.valid())
		SKIP("no Vulkan driver available");

	uint32_t queueIndex = 0;
	const acm::GPU* gpu = acmtest::selectGraphicsGPU(instance, queueIndex);
	if (!gpu)
		SKIP("no graphics-capable queue family");

	acm::Device device = instance.createDevice(*gpu, queueIndex);
	REQUIRE(device.valid());

	constexpr uint32_t kSize = 64;
	const acm::Extent2D extent{kSize, kSize};

	// Fill a CPU buffer with solid green. B8G8R8A8_Unorm memory order is [B,G,R,A].
	std::vector<uint8_t> pixels(size_t(kSize) * kSize * 4);
	for (size_t i = 0; i < pixels.size(); i += 4)
	{
		pixels[i + 0] = 0;	 // B
		pixels[i + 1] = 255; // G
		pixels[i + 2] = 0;	 // R
		pixels[i + 3] = 255; // A
	}

	acm::Texture texA = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	REQUIRE(texA.valid());
	texA.upload(pixels.data(), pixels.size()); // leaves texA in SHADER_READ_ONLY

	// Sample texA into texB, then read texB back.
	acm::Texture texB = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::RenderTarget targetB = device.createRenderTarget(texB, acm::RenderTargetFinish::CopySrc);
	REQUIRE(targetB.valid());

	acm::Sampler sampler = device.createSampler();
	acm::DescriptorSetLayout layout = device.createDescriptorSetLayout(1);
	acm::DescriptorSet descriptors = device.createDescriptorSet(layout);
	REQUIRE(descriptors.valid());
	descriptors.setTexture(0, texA, sampler);

	acm::PipelineConfig config;
	config.vertex = device.createShader(acmtest::fullscreenVertSpirv());
	config.fragment = device.createShader(acmtest::sampleTextureFragSpirv());
	config.target = targetB;
	config.descriptorLayout = layout;
	acm::Pipeline pipeline = device.createPipeline(config);
	REQUIRE(pipeline.valid());

	acm::Buffer readback = device.createBuffer(size_t(kSize) * kSize * 4, acm::BufferUsage::TransferDst);
	REQUIRE(readback.valid());

	acm::CommandPool pool = device.createCommandPool();
	acm::CommandBuffer cmd = pool.allocate();
	REQUIRE(cmd.valid());

	cmd.begin();
	cmd.beginRendering(targetB);
	cmd.setViewportAndScissor(extent);
	cmd.bindPipeline(pipeline);
	cmd.bindDescriptorSet(pipeline, descriptors);
	cmd.draw(3);
	cmd.endRendering();
	cmd.copyTextureToBuffer(texB, readback);
	cmd.end();

	REQUIRE_FALSE(device.submitSync(cmd));

	// Center of B sampled A's uploaded green. B8G8R8A8 layout is [B,G,R,A].
	const auto* out = static_cast<const uint8_t*>(readback.map());
	REQUIRE(out != nullptr);
	const size_t center = (size_t(kSize / 2) * kSize + kSize / 2) * 4;
	const uint8_t b = out[center + 0];
	const uint8_t g = out[center + 1];
	const uint8_t r = out[center + 2];
	readback.unmap();

	REQUIRE(g > 200);
	REQUIRE(r < 60);
	REQUIRE(b < 60);
}
