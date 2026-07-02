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
#include <tuple>

// Integration: depth testing. A depth-enabled offscreen target, a depth-testing
// pipeline (compare LESS), and the same triangle drawn twice at different depths via
// a uniform z-translation: a NEAR green one (z=0.3) first, then a FAR red one (z=0.7)
// second. With depth on, the far red fails the test against the already-written near
// green, so the center stays green. Without depth the later (red) draw would win — so
// a green center is the proof depth testing is active. Surface-free, runs anywhere.

namespace
{
	// Column-major (GLSL mat4) translation along z, so gl_Position.z = z for the
	// shader's xy triangle — i.e. it sets the fragment's depth.
	struct Mat4
	{
		float m[16];
	};
	Mat4 translateZ(float z)
	{
		return Mat4{{1, 0, 0, 0,
					 0, 1, 0, 0,
					 0, 0, 1, 0,
					 0, 0, z, 1}};
	}
} // namespace

TEST_CASE("depth testing rejects farther fragments", "[acm][gpu]")
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

	// The whole near-green-then-far-red scene at a given sample count. Run at 1x and
	// (when supported) 4x, so the depth + depth-clear-index logic is exercised under
	// MSAA too (where a resolve attachment shifts the depth attachment's index).
	auto runScene = [&](acm::SampleCount samples)
	{
		acm::Texture color = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
		acm::RenderTarget target = device.createRenderTarget(color, acm::RenderTargetFinish::CopySrc, /*depth*/ true, samples);
		REQUIRE(target.valid());
		REQUIRE(target.hasDepth());

		acm::DescriptorSetLayout layout = device.createDescriptorSetLayout({
			{0, acm::DescriptorType::UniformBuffer, acm::ShaderStage::Vertex},
			{1, acm::DescriptorType::UniformBuffer, acm::ShaderStage::Fragment},
		});
		REQUIRE(layout.valid());

		acm::PipelineConfig config;
		config.vertex = device.createShader(acmtest::uniformTransformVertSpirv());
		config.fragment = device.createShader(acmtest::uniformColorFragSpirv());
		config.target = target;
		config.descriptorLayout = layout;
		config.depthTest = true;
		acm::Pipeline pipeline = device.createPipeline(config);
		REQUIRE(pipeline.valid());

		// Build the two draws: near+green and far+red.
		auto makeSet = [&](float z, const float (&rgba)[4])
		{
			const Mat4 mvp = translateZ(z);
			acm::Buffer xform = device.createBuffer(sizeof(mvp), acm::BufferUsage::Uniform);
			acm::Buffer col = device.createBuffer(sizeof(rgba), acm::BufferUsage::Uniform);
			xform.write(&mvp, sizeof(mvp));
			col.write(rgba, sizeof(rgba));
			acm::DescriptorSet set = device.createDescriptorSet(layout);
			set.setBuffer(0, xform);
			set.setBuffer(1, col);
			return std::make_tuple(set, xform, col);
		};
		const float green[4] = {0.0f, 1.0f, 0.0f, 1.0f};
		const float red[4] = {1.0f, 0.0f, 0.0f, 1.0f};
		auto [nearSet, nearXform, nearCol] = makeSet(0.3f, green);
		auto [farSet, farXform, farCol] = makeSet(0.7f, red);

		acm::Buffer readback = device.createBuffer(size_t(kSize) * kSize * 4, acm::BufferUsage::TransferDst);
		REQUIRE(readback.valid());

		acm::CommandPool pool = device.createCommandPool();
		acm::CommandBuffer cmd = pool.allocate();
		REQUIRE(cmd.valid());

		cmd.begin();
		cmd.beginRendering(target);
		cmd.setViewportAndScissor(extent);
		cmd.bindPipeline(pipeline);
		cmd.bindDescriptorSet(pipeline, nearSet); // near green first
		cmd.draw(3);
		cmd.bindDescriptorSet(pipeline, farSet); // far red second — must be depth-rejected
		cmd.draw(3);
		cmd.endRendering();
		cmd.copyTextureToBuffer(color, readback);
		cmd.end();

		REQUIRE_FALSE(device.submitSync(cmd));

		// Center must be the near triangle's green: the far red was drawn last but
		// failed the depth test. B8G8R8A8 layout is [B,G,R,A].
		const auto* pixels = static_cast<const uint8_t*>(readback.map());
		REQUIRE(pixels != nullptr);
		const size_t center = (size_t(kSize / 2) * kSize + kSize / 2) * 4;
		const uint8_t b = pixels[center + 0];
		const uint8_t g = pixels[center + 1];
		const uint8_t r = pixels[center + 2];
		readback.unmap();

		REQUIRE(g > 200);
		REQUIRE(r < 60);
		REQUIRE(b < 60);
	};

	runScene(acm::SampleCount::One);
	if (static_cast<int>(device.maxSampleCount()) >= static_cast<int>(acm::SampleCount::Four))
		runScene(acm::SampleCount::Four); // depth + MSAA together
}
