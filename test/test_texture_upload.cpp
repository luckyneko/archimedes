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

namespace
{
	struct Bgr
	{
		uint8_t b;
		uint8_t g;
		uint8_t r;
	};

	// Solid green, B8G8R8A8_Unorm memory order [B,G,R,A].
	std::vector<uint8_t> greenPixels(acm::Extent2D extent)
	{
		std::vector<uint8_t> pixels(size_t(extent.width) * extent.height * 4);
		for (size_t i = 0; i < pixels.size(); i += 4)
		{
			pixels[i + 0] = 0;	 // B
			pixels[i + 1] = 255; // G
			pixels[i + 2] = 0;	 // R
			pixels[i + 3] = 255; // A
		}
		return pixels;
	}

	// Samples `source` (in SHADER_READ_ONLY) fullscreen into a fresh B8G8R8A8 texture
	// and returns the center pixel read back from it.
	Bgr sampleCenter(acm::Device& device, const acm::Texture& source, acm::Extent2D extent)
	{
		acm::Texture texB = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
		acm::RenderTarget targetB = device.createRenderTarget(texB, acm::RenderTargetConfig{acm::RenderTargetFinish::CopySrc});
		REQUIRE(targetB.valid());

		acm::Sampler sampler = device.createSampler();
		acm::DescriptorSetLayout layout = device.createDescriptorSetLayout(1);
		acm::DescriptorSet descriptors = device.createDescriptorSet(layout);
		REQUIRE(descriptors.valid());
		descriptors.setTexture(0, source, sampler);

		acm::PipelineShaders shaders;
		shaders.vertex = device.createShader(acmtest::fullscreenVertSpirv());
		shaders.fragment = device.createShader(acmtest::sampleTextureFragSpirv());
		acm::PipelineConfig config;
		config.descriptorLayout = layout;
		acm::Pipeline pipeline = device.createPipeline(shaders, targetB, config);
		REQUIRE(pipeline.valid());

		acm::Buffer readback = device.createBuffer(size_t(extent.width) * extent.height * 4, acm::BufferUsage::TransferDst);
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

		// B8G8R8A8 layout is [B,G,R,A].
		const auto* out = static_cast<const uint8_t*>(readback.map());
		REQUIRE(out != nullptr);
		const size_t center = (size_t(extent.height / 2) * extent.width + extent.width / 2) * 4;
		const Bgr pixel{out[center + 0], out[center + 1], out[center + 2]};
		readback.unmap();
		return pixel;
	}
} // namespace

TEST_CASE("uploaded texture pixels can be sampled", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	acmtest::requireInstance(instance);

	uint32_t queueIndex = 0;
	const acm::DeviceInfo* gpu = acmtest::selectGraphicsDevice(instance, queueIndex);

	acm::Device device = instance.createDevice(*gpu, queueIndex);
	REQUIRE(device.valid());

	constexpr uint32_t kSize = 64;
	const acm::Extent2D extent{kSize, kSize};
	const std::vector<uint8_t> pixels = greenPixels(extent);

	acm::Texture texA = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	REQUIRE(texA.valid());
	REQUIRE_FALSE(texA.upload(pixels.data(), pixels.size())); // leaves texA in SHADER_READ_ONLY

	// Center of B sampled A's uploaded green.
	const Bgr center = sampleCenter(device, texA, extent);
	REQUIRE(center.g > 200);
	REQUIRE(center.r < 60);
	REQUIRE(center.b < 60);
}

// Texture::upload validates before it touches the GPU: a size other than width * height *
// bytes-per-pixel (a short buffer used to make the copy read past the staging buffer), a
// null source, and any depth texture are errors, and each leaves the texture valid and
// untouched, so a correct upload afterwards still lands and samples back.
TEST_CASE("Texture::upload rejects bad input and leaves the texture usable", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	acmtest::requireInstance(instance);

	uint32_t queueIndex = 0;
	const acm::DeviceInfo* gpu = acmtest::selectGraphicsDevice(instance, queueIndex);

	acm::Device device = instance.createDevice(*gpu, queueIndex);
	REQUIRE(device.valid());

	constexpr uint32_t kSize = 16;
	const acm::Extent2D extent{kSize, kSize};
	const size_t exactSize = size_t(kSize) * kSize * 4;
	std::vector<uint8_t> pixels = greenPixels(extent);
	pixels.resize(exactSize + 4); // room for the oversize case; only exactSize is ever uploaded

	acm::Texture texture = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	REQUIRE(texture.valid());
	REQUIRE(texture.upload(pixels.data(), exactSize - 4));
	REQUIRE(texture.upload(pixels.data(), exactSize + 4));
	REQUIRE(texture.upload(pixels.data(), 0));
	REQUIRE(texture.upload(nullptr, exactSize));
	REQUIRE(texture.valid());

	// The expected size follows the format: a 16-byte texel needs four times the bytes.
	acm::Texture wide = device.createTexture(acm::Format::R32G32B32A32_Sfloat, extent);
	REQUIRE(wide.valid());
	const std::vector<uint8_t> widePixels(size_t(kSize) * kSize * 16);
	REQUIRE(wide.upload(widePixels.data(), exactSize));
	REQUIRE_FALSE(wide.upload(widePixels.data(), widePixels.size()));

	// Depth textures cannot be uploaded, even with a correctly sized source.
	acm::Texture depth = device.createTexture(acm::Format::D32_Sfloat, extent);
	REQUIRE(depth.valid());
	const std::vector<uint8_t> depthPixels(size_t(kSize) * kSize * 4);
	REQUIRE(depth.upload(depthPixels.data(), depthPixels.size()));
	REQUIRE(depth.valid());

	// The rejected uploads left the color texture usable.
	REQUIRE_FALSE(texture.upload(pixels.data(), exactSize));
	const Bgr center = sampleCenter(device, texture, extent);
	REQUIRE(center.g > 200);
	REQUIRE(center.r < 60);
	REQUIRE(center.b < 60);
}
