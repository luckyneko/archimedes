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

// Integration: mipmap generation. A left-red / right-blue texture is uploaded both
// mipmapped and not, then sampled with a forced-high LOD (textureLod 10) — which reads
// the *smallest* mip. With mips, that top level is the global average (purple) no
// matter the UV; without mips the LOD clamps to level 0, so a UV in the left half
// reads pure red. So mipped→purple vs unmipped→red at the same pixel proves the mip
// chain was generated (and is sampled). Surface-free.

TEST_CASE("mipmaps are generated and sampled", "[acm][gpu]")
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

	constexpr uint32_t kSize = 64;
	const acm::Extent2D extent{kSize, kSize};

	// Level 0: left half red, right half blue. B8G8R8A8_Unorm memory order is [B,G,R,A].
	std::vector<uint8_t> pixels(size_t(kSize) * kSize * 4);
	for (uint32_t y = 0; y < kSize; ++y)
		for (uint32_t x = 0; x < kSize; ++x)
		{
			uint8_t* p = &pixels[(size_t(y) * kSize + x) * 4];
			const bool left = x < kSize / 2;
			p[0] = left ? 0 : 255; // B
			p[1] = 0;			   // G
			p[2] = left ? 255 : 0; // R
			p[3] = 255;			   // A
		}

	acm::Sampler sampler = device.createSampler();
	acm::DescriptorSetLayout layout = device.createDescriptorSetLayout(1);

	// Samples `tex` with a forced high LOD into a texture, returns the readback buffer.
	auto sampleTopMip = [&](acm::Texture tex)
	{
		acm::DescriptorSet descriptors = device.createDescriptorSet(layout);
		descriptors.setTexture(0, tex, sampler);

		acm::Texture out = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
		acm::RenderTarget target = device.createRenderTarget(out, acm::RenderTargetFinish::CopySrc);
		REQUIRE(target.valid());

		acm::PipelineConfig config;
		config.vertex = device.createShader(acmtest::fullscreenVertSpirv());
		config.fragment = device.createShader(acmtest::sampleTextureLodFragSpirv());
		config.target = target;
		config.descriptorLayout = layout;
		acm::Pipeline pipeline = device.createPipeline(config);
		REQUIRE(pipeline.valid());

		acm::Buffer readback = device.createBuffer(size_t(kSize) * kSize * 4, acm::BufferUsage::TransferDst);

		acm::CommandPool pool = device.createCommandPool();
		acm::CommandBuffer cmd = pool.allocate();
		cmd.begin();
		cmd.beginRendering(target);
		cmd.setViewportAndScissor(extent);
		cmd.bindPipeline(pipeline);
		cmd.bindDescriptorSet(pipeline, descriptors);
		cmd.draw(3);
		cmd.endRendering();
		cmd.copyTextureToBuffer(out, readback);
		cmd.end();

		REQUIRE_FALSE(device.submitSync(cmd));
		return readback;
	};

	acm::Texture mipped = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent, /*mipmapped*/ true);
	acm::Texture flat = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent, /*mipmapped*/ false);
	REQUIRE(mipped.mipLevels() > 1);
	REQUIRE(flat.mipLevels() == 1);
	REQUIRE_FALSE(device.createRenderTarget(mipped, acm::RenderTargetFinish::CopySrc).valid());
	mipped.upload(pixels.data(), pixels.size()); // generates the chain
	flat.upload(pixels.data(), pixels.size());

	acm::Buffer mippedOut = sampleTopMip(mipped);
	acm::Buffer flatOut = sampleTopMip(flat);

	// Inspect a pixel in the left (red) half of the output.
	const size_t px = (size_t(kSize / 2) * kSize + kSize / 4) * 4;
	const auto* m = static_cast<const uint8_t*>(mippedOut.map());
	const uint8_t mb = m[px + 0], mg = m[px + 1], mr = m[px + 2];
	mippedOut.unmap();
	const auto* f = static_cast<const uint8_t*>(flatOut.map());
	const uint8_t fb = f[px + 0], fg = f[px + 1], fr = f[px + 2];
	flatOut.unmap();

	// Mipped: top mip is the red+blue average (purple) everywhere — red and blue both ~half.
	REQUIRE(mr > 80);
	REQUIRE(mr < 180);
	REQUIRE(mb > 80);
	REQUIRE(mb < 180);
	REQUIRE(mg < 60);

	// Unmipped: LOD clamps to level 0, and this UV is in the red half — pure red.
	REQUIRE(fr > 200);
	REQUIRE(fb < 60);
	REQUIRE(fg < 60);
}
