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

// Integration: configurable pipeline state, proved via face culling. The same red
// triangle is drawn three ways into three textures: no cull (baseline — center red),
// and back-face cull at each winding. With culling on, exactly one winding leaves the
// triangle front-facing (red) and the other culls it away (clear/black) — so flipping
// FrontFace flips visibility. Surface-free, runs anywhere with a graphics queue.

namespace
{
	bool isRed(const uint8_t* p)
	{
		return p[2] > 200 && p[1] < 60 && p[0] < 60; // [B,G,R,A]
	}
	bool isBlack(const uint8_t* p)
	{
		return p[2] < 60 && p[1] < 60 && p[0] < 60;
	}
} // namespace

TEST_CASE("PipelineConfig presets expose named default states", "[acm]")
{
	const acm::PipelineConfig defaults = acm::PipelineConfig::Default();
	REQUIRE(defaults.topology == acm::Topology::TriangleList);
	REQUIRE(defaults.cullMode == acm::CullMode::None);
	REQUIRE(defaults.frontFace == acm::FrontFace::Clockwise);
	REQUIRE(defaults.blend == acm::BlendMode::Opaque);
	REQUIRE(defaults.polygonMode == acm::PolygonMode::Fill);
	REQUIRE(defaults.lineWidth == 1.0f);
	REQUIRE(defaults.minSampleShading == 0.0f);
	REQUIRE_FALSE(defaults.depth.test);
	REQUIRE_FALSE(defaults.depth.write);

	const acm::PipelineConfig mesh = acm::PipelineConfig::Mesh3D();
	REQUIRE(mesh.topology == acm::Topology::TriangleList);
	REQUIRE(mesh.cullMode == acm::CullMode::Back);
	REQUIRE(mesh.frontFace == acm::FrontFace::Clockwise);
	REQUIRE(mesh.blend == acm::BlendMode::Opaque);

	const acm::PipelineConfig sprite = acm::PipelineConfig::Sprite2D();
	REQUIRE(sprite.topology == acm::Topology::TriangleStrip);
	REQUIRE(sprite.cullMode == acm::CullMode::None);
	REQUIRE(sprite.blend == acm::BlendMode::AlphaBlend);

	const acm::PipelineConfig wire = acm::PipelineConfig::Wireframe(2.0f);
	REQUIRE(wire.cullMode == acm::CullMode::None);
	REQUIRE(wire.polygonMode == acm::PolygonMode::Line);
	REQUIRE(wire.lineWidth == 2.0f);
}

TEST_CASE("pipeline cull mode / front face take effect", "[acm][gpu]")
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

	// Three textures + targets, one per pipeline variant.
	struct Variant
	{
		acm::CullMode cull;
		acm::FrontFace front;
		acm::Texture tex;
		acm::RenderTarget target;
		acm::Pipeline pipeline;
		acm::Buffer readback;
	};
	Variant variants[3] = {
		{acm::CullMode::None, acm::FrontFace::Clockwise, {}, {}, {}, {}},
		{acm::CullMode::Back, acm::FrontFace::Clockwise, {}, {}, {}, {}},
		{acm::CullMode::Back, acm::FrontFace::CounterClockwise, {}, {}, {}, {}},
	};

	acm::CommandPool pool = device.createCommandPool();
	acm::CommandBuffer cmd = pool.allocate();
	REQUIRE(cmd.valid());
	cmd.begin();

	for (auto& v : variants)
	{
		v.tex = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
		v.target = device.createRenderTarget(v.tex, acm::RenderTargetConfig{acm::RenderTargetFinish::CopySrc});
		REQUIRE(v.target.valid());

		acm::PipelineShaders shaders;
		shaders.vertex = device.createShader(acmtest::triangleVertSpirv());
		shaders.fragment = device.createShader(acmtest::triangleFragSpirv());
		const bool mesh3D = v.cull == acm::CullMode::Back && v.front == acm::FrontFace::Clockwise;
		acm::PipelineConfig config = mesh3D ? acm::PipelineConfig::Mesh3D() : acm::PipelineConfig::Default();
		if (!mesh3D)
		{
			config.cullMode = v.cull;
			config.frontFace = v.front;
		}
		v.pipeline = device.createPipeline(shaders, v.target, config);
		REQUIRE(v.pipeline.valid());

		v.readback = device.createBuffer(size_t(kSize) * kSize * 4, acm::BufferUsage::TransferDst);

		cmd.beginRendering(v.target); // clears to black
		cmd.setViewportAndScissor(extent);
		cmd.bindPipeline(v.pipeline);
		cmd.draw(3);
		cmd.endRendering();
		cmd.copyTextureToBuffer(v.tex, v.readback);
	}

	cmd.end();

	REQUIRE_FALSE(device.submitSync(cmd));

	const size_t center = (size_t(kSize / 2) * kSize + kSize / 2) * 4;
	const uint8_t* none = static_cast<const uint8_t*>(variants[0].readback.map()) + center;
	const uint8_t* cw = static_cast<const uint8_t*>(variants[1].readback.map()) + center;
	const uint8_t* ccw = static_cast<const uint8_t*>(variants[2].readback.map()) + center;

	// No cull: the triangle covers the center, so it's red — the baseline.
	REQUIRE(isRed(none));

	// Back cull: exactly one winding is front-facing (red); the other is culled (black).
	REQUIRE(isRed(cw) != isRed(ccw));
	REQUIRE((isRed(cw) ? isBlack(ccw) : isBlack(cw)));

	variants[0].readback.unmap();
	variants[1].readback.unmap();
	variants[2].readback.unmap();
}
