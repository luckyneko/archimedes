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

// Regression: a graphics pipeline bound in one rendering pass must not be judged
// against the *next* pass's render target. The mirror example renders a cube into a
// depth-bearing offscreen target, then samples it in a depth-less pass (the swapchain).
// A pipeline binding is scoped to its pass, so endRendering forgets it; without that,
// the depth-bearing cube pipeline stayed "bound" and made the depth-less second
// beginRendering fail its compatibility check ("bound graphics pipeline is incompatible
// with render target"), so the second pass never ran — a blank window. Two depth-less
// targets (as test_sampler uses) never exercise this; the first target needs depth so its
// pipeline's signature differs from the second target's.

TEST_CASE("a pipeline bound in a depth pass doesn't block a later depth-less pass", "[acm][gpu]")
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

	// Pass 1 target: depth-bearing (like mirror's offscreen RTT).
	acm::Texture colorA = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::RenderTarget targetA = device.createRenderTarget(colorA, acm::RenderTargetConfig{acm::RenderTargetFinish::Sampled, true});
	acm::PipelineConfig configA;
	configA.depth = acm::DepthState::TestWrite();
	acm::Pipeline pipeA = device.createPipeline(acm::PipelineShaders{device.createShader(acmtest::triangleVertSpirv()),
																	 device.createShader(acmtest::triangleFragSpirv())},
												targetA, configA);
	REQUIRE(targetA.valid());
	REQUIRE(pipeA.valid());

	// Pass 2 target: no depth (like the swapchain), left ready to copy out.
	acm::Texture colorB = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::RenderTarget targetB = device.createRenderTarget(colorB, acm::RenderTargetConfig{acm::RenderTargetFinish::CopySrc});
	acm::Pipeline pipeB = device.createPipeline(acm::PipelineShaders{device.createShader(acmtest::triangleVertSpirv()),
																	 device.createShader(acmtest::triangleFragSpirv())},
												targetB);
	REQUIRE(targetB.valid());
	REQUIRE(pipeB.valid());

	acm::Buffer readback = device.createBuffer(size_t(kSize) * kSize * 4, acm::BufferUsage::TransferDst);
	REQUIRE(readback.valid());

	acm::CommandPool pool = device.createCommandPool();
	acm::CommandBuffer cmd = pool.allocate();
	REQUIRE(cmd.valid());
	REQUIRE_FALSE(cmd.begin());

	// Pass 1: draw into the depth target, binding the depth pipeline.
	REQUIRE_FALSE(cmd.beginRendering(targetA));
	cmd.setViewportAndScissor(extent);
	REQUIRE_FALSE(cmd.bindPipeline(pipeA));
	REQUIRE_FALSE(cmd.draw(3));
	cmd.endRendering();

	// Pass 2: beginning the depth-less target must succeed even though pipeA (a depth
	// pipeline) was the last thing bound — the regression assertion.
	REQUIRE_FALSE(cmd.beginRendering(targetB));
	cmd.setViewportAndScissor(extent);
	REQUIRE_FALSE(cmd.bindPipeline(pipeB));
	REQUIRE_FALSE(cmd.draw(3));
	cmd.endRendering();
	cmd.copyTextureToBuffer(colorB, readback);
	REQUIRE_FALSE(cmd.error());
	REQUIRE_FALSE(cmd.end());
	REQUIRE_FALSE(device.submitSync(cmd));

	// The second pass actually rendered: its center is the red triangle, not the
	// cleared black (which is what a skipped second pass would have left).
	const auto* pixels = static_cast<const uint8_t*>(readback.map());
	REQUIRE(pixels != nullptr);
	const size_t center = (size_t(kSize / 2) * kSize + kSize / 2) * 4;
	const uint8_t b = pixels[center + 0]; // B8G8R8A8 layout is [B,G,R,A]
	const uint8_t g = pixels[center + 1];
	const uint8_t r = pixels[center + 2];
	readback.unmap();
	REQUIRE(r > 200);
	REQUIRE(g < 60);
	REQUIRE(b < 60);
}
