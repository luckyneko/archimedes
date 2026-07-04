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

// Integration: builds an acm::Shader + acm::Pipeline against a headless
// swapchain target, using the precompiled SPIR-V in test_spirv.h. SKIPs
// without a live driver / headless surface / swapchain support.

TEST_CASE("Shader rejects empty SPIR-V, accepts valid", "[acm][gpu]")
{
	acmtest::HeadlessStack s;
	if (!acmtest::buildHeadlessStack(s))
		return;

	REQUIRE_FALSE(s.device.createShader(std::vector<char>{}).valid());

	acm::Shader vert = s.device.createShader(acmtest::triangleVertSpirv());
	REQUIRE(vert.valid());
	acm::Shader retained = vert;
	vert.reset();
	REQUIRE_FALSE(vert.valid());
	REQUIRE(retained.valid());
}

TEST_CASE("Pipeline builds from shaders + render target", "[acm][gpu]")
{
	acmtest::HeadlessStack s;
	if (!acmtest::buildHeadlessStack(s))
		return;

	acm::Shader vert = s.device.createShader(acmtest::triangleVertSpirv());
	acm::Shader frag = s.device.createShader(acmtest::triangleFragSpirv());
	REQUIRE(vert.valid());
	REQUIRE(frag.valid());

	const acm::PipelineShaders shaders{vert, frag};
	acm::Pipeline pipeline = s.device.createPipeline(shaders, s.swapChain.renderTarget(0));
	REQUIRE(pipeline.valid());

	acm::PipelineConfig incompatible;
	incompatible.depth = acm::DepthState::TestWrite();
	REQUIRE_FALSE(s.device.createPipeline(shaders, s.swapChain.renderTarget(0), incompatible).valid());

	acm::Texture texture = s.device.createTexture(acm::Format::B8G8R8A8_Unorm, acm::Extent2D{32, 32});
	acm::RenderTarget depthTarget = s.device.createRenderTarget(texture, acm::RenderTargetConfig{acm::RenderTargetFinish::CopySrc, true});
	REQUIRE(depthTarget.valid());
	acm::PipelineConfig writeOnlyDepth;
	writeOnlyDepth.depth.write = true;
	REQUIRE_FALSE(s.device.createPipeline(shaders, depthTarget, writeOnlyDepth).valid());

	acm::Pipeline retained = pipeline;
	pipeline.reset();
	REQUIRE_FALSE(pipeline.valid());
	REQUIRE(retained.valid());
	pipeline = std::move(retained);
	REQUIRE(pipeline.valid());
	REQUIRE_FALSE(retained.valid());

	// The shaders are only needed at creation time; dropping them must not affect
	// the already-built pipeline.
	vert.reset();
	frag.reset();
	REQUIRE(pipeline.valid());
}

TEST_CASE("Pipeline is invalid when a shader is", "[acm][gpu]")
{
	acmtest::HeadlessStack s;
	if (!acmtest::buildHeadlessStack(s))
		return;

	acm::Shader frag = s.device.createShader(acmtest::triangleFragSpirv());
	REQUIRE(frag.valid());

	// A null vertex shader can't build a pipeline.
	acm::Pipeline pipeline = s.device.createPipeline(acm::PipelineShaders{acm::Shader(), frag}, s.swapChain.renderTarget(0));
	REQUIRE_FALSE(pipeline.valid());
}

TEST_CASE("Command buffer refuses a pipeline incompatible with the active render target", "[acm][gpu]")
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

	acm::Texture noDepthTexture = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::RenderTarget noDepthTarget = device.createRenderTarget(noDepthTexture, acm::RenderTargetConfig{acm::RenderTargetFinish::CopySrc});
	REQUIRE(noDepthTarget.valid());

	acm::Shader vert = device.createShader(acmtest::triangleVertSpirv());
	acm::Shader frag = device.createShader(acmtest::triangleFragSpirv());
	REQUIRE(vert.valid());
	REQUIRE(frag.valid());

	acm::Pipeline noDepthPipeline = device.createPipeline(acm::PipelineShaders{vert, frag}, noDepthTarget);
	REQUIRE(noDepthPipeline.valid());

	acm::Texture depthTexture = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::RenderTarget depthTarget = device.createRenderTarget(depthTexture, acm::RenderTargetConfig{acm::RenderTargetFinish::CopySrc, true});
	REQUIRE(depthTarget.valid());

	acm::Buffer readback = device.createBuffer(size_t(kSize) * kSize * 4, acm::BufferUsage::TransferDst);
	REQUIRE(readback.valid());

	acm::CommandPool pool = device.createCommandPool();
	acm::CommandBuffer cmd = pool.allocate();
	REQUIRE(cmd.valid());
	REQUIRE_FALSE(cmd.begin());
	cmd.beginRendering(depthTarget); // clears to black
	cmd.setViewportAndScissor(extent);
	REQUIRE(cmd.bindPipeline(noDepthPipeline));
	REQUIRE(cmd.draw(3));
	REQUIRE(cmd.error());
	cmd.endRendering();
	cmd.copyTextureToBuffer(depthTexture, readback);
	REQUIRE_FALSE(cmd.end());

	REQUIRE_FALSE(device.submitSync(cmd));

	const size_t center = (size_t(kSize / 2) * kSize + kSize / 2) * 4;
	const uint8_t* pixel = static_cast<const uint8_t*>(readback.map()) + center;
	REQUIRE(pixel[2] < 60);
	REQUIRE(pixel[1] < 60);
	REQUIRE(pixel[0] < 60);
	readback.unmap();
}
