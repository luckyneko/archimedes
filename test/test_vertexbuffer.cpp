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

// Integration: draws an indexed triangle whose geometry comes from a vertex
// buffer + index buffer (not gl_VertexIndex), into an offscreen texture, then
// reads the center pixel back. Verifies vertex-input pipeline state +
// bindVertexBuffer/bindIndexBuffer/drawIndexed. Surface-free, runs anywhere.

namespace
{
	struct Vertex
	{
		float pos[2];
		float color[3];
	};
} // namespace

TEST_CASE("indexed draw from vertex + index buffers", "[acm][gpu]")
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

	acm::Texture texture = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::RenderTarget target = device.createRenderTarget(texture, acm::RenderTargetFinish::CopySrc);
	REQUIRE(target.valid());

	// Geometry in buffers: a red triangle covering the center, drawn by index.
	const Vertex vertices[3] = {
		{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
		{{0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
		{{-0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
	};
	const uint32_t indices[3] = {0, 1, 2};

	acm::Buffer vertexBuffer = device.createBuffer(sizeof(vertices), acm::BufferUsage::Vertex);
	acm::Buffer indexBuffer = device.createBuffer(sizeof(indices), acm::BufferUsage::Index);
	REQUIRE(vertexBuffer.valid());
	REQUIRE(indexBuffer.valid());
	vertexBuffer.write(vertices, sizeof(vertices));
	indexBuffer.write(indices, sizeof(indices));

	acm::PipelineConfig config;
	config.vertex = device.createShader(acmtest::vertexColorVertSpirv());
	config.fragment = device.createShader(acmtest::vertexColorFragSpirv());
	config.target = target;
	config.vertexLayout.stride = sizeof(Vertex);
	config.vertexLayout.attributes = {
		{0, acm::Format::R32G32_Sfloat, offsetof(Vertex, pos)},
		{1, acm::Format::R32G32B32_Sfloat, offsetof(Vertex, color)},
	};
	acm::Pipeline pipeline = device.createPipeline(config);
	REQUIRE(pipeline.valid());

	acm::Buffer readback = device.createBuffer(size_t(kSize) * kSize * 4, acm::BufferUsage::TransferDst);
	REQUIRE(readback.valid());

	acm::CommandPool pool = device.createCommandPool();
	acm::CommandBuffer cmd = pool.allocate();
	REQUIRE(cmd.valid());

	cmd.begin();
	cmd.beginRendering(target);
	cmd.setViewportAndScissor(extent);
	cmd.bindPipeline(pipeline);
	cmd.bindVertexBuffer(vertexBuffer);
	cmd.bindIndexBuffer(indexBuffer);
	cmd.drawIndexed(3);
	cmd.endRendering();
	cmd.copyTextureToBuffer(texture, readback);
	cmd.end();

	REQUIRE_FALSE(device.submitSync(cmd));

	// The buffer-fed triangle covers the center, so it must be its vertex red.
	const auto* pixels = static_cast<const uint8_t*>(readback.map());
	REQUIRE(pixels != nullptr);
	const size_t center = (size_t(kSize / 2) * kSize + kSize / 2) * 4;
	const uint8_t b = pixels[center + 0];
	const uint8_t g = pixels[center + 1];
	const uint8_t r = pixels[center + 2];
	readback.unmap();

	REQUIRE(r > 200);
	REQUIRE(g < 60);
	REQUIRE(b < 60);
}
