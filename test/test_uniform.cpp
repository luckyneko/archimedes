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

// Integration: the uniform-buffer / descriptor path. A single descriptor set holds
// two uniform buffers of different types-of-use and stages — a mat4 transform read
// by the vertex shader (binding 0) and a vec4 color read by the fragment shader
// (binding 1). The triangle is rendered into a texture and read back: the center
// pixel must be the *uniform* color (green), proving the fragment-stage uniform was
// consumed, while the identity transform keeps the triangle covering the center,
// proving the vertex-stage uniform was wired in. Surface-free, runs anywhere.

TEST_CASE("a uniform buffer drives shader output", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	if (!instance.valid())
		SKIP("no Vulkan driver available");

	uint32_t queueIdx = 0;
	const acm::GPU* gpu = acmtest::selectGraphicsGPU(instance, queueIdx);
	if (!gpu)
		SKIP("no graphics-capable queue family");

	acm::Device device = instance.createDevice(*gpu, queueIdx);
	REQUIRE(device.valid());

	constexpr uint32_t kSize = 64;
	const acm::Extent2D extent{kSize, kSize};

	acm::Texture texture = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::RenderTarget target = device.createRenderTarget(texture, acm::RenderTargetFinish::CopySrc);
	REQUIRE(target.valid());

	// Two uniforms: a vertex-stage mat4 (identity) and a fragment-stage color (green).
	const float identity[16] = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f};
	const float green[4] = {0.0f, 1.0f, 0.0f, 1.0f};

	acm::Buffer transformBuf = device.createBuffer(sizeof(identity), acm::BufferUsage::Uniform);
	acm::Buffer colorBuf = device.createBuffer(sizeof(green), acm::BufferUsage::Uniform);
	REQUIRE(transformBuf.valid());
	REQUIRE(colorBuf.valid());
	transformBuf.write(identity, sizeof(identity));
	colorBuf.write(green, sizeof(green));

	acm::DescriptorSetLayout layout = device.createDescriptorSetLayout({
		{0, acm::DescriptorType::UniformBuffer, acm::ShaderStage::Vertex},
		{1, acm::DescriptorType::UniformBuffer, acm::ShaderStage::Fragment},
	});
	REQUIRE(layout.valid());
	acm::DescriptorSet descriptors = device.createDescriptorSet(layout);
	REQUIRE(descriptors.valid());
	descriptors.setBuffer(0, transformBuf);
	descriptors.setBuffer(1, colorBuf);

	acm::PipelineConfig config;
	config.vertex = device.createShader(acmtest::uniformTransformVertSpirv());
	config.fragment = device.createShader(acmtest::uniformColorFragSpirv());
	config.target = target;
	config.descriptorLayout = layout;
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
	cmd.bindDescriptorSet(pipeline, descriptors);
	cmd.draw(3);
	cmd.endRendering();
	cmd.copyTextureToBuffer(texture, readback);
	cmd.end();

	REQUIRE_FALSE(device.submitSync(cmd));

	// Center pixel must be the uniform-supplied green. B8G8R8A8 layout is [B,G,R,A].
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
}
