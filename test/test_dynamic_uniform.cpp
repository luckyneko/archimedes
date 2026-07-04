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
#include <cstring>
#include <vector>

// Integration: a dynamic uniform buffer. One buffer holds two objects' colors, each at an
// aligned stride; the per-draw dynamic offset picks which the fragment shader reads.
// Element 0 is red, element 1 (at the aligned offset) is green. The draw binds with
// dynamicOffset = stride, so the pixel must be green — proving the offset selected element
// 1, not element 0. Surface-free, runs anywhere with a graphics queue.

TEST_CASE("a dynamic uniform offset selects the right slice", "[acm][gpu]")
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

	// Pack two vec4 colors, each at a stride that satisfies the device's dynamic-offset
	// alignment requirement.
	const size_t elementSize = sizeof(float) * 4;
	const size_t align = device.minUniformBufferOffsetAlignment();
	const size_t stride = align > elementSize ? align : elementSize;
	std::vector<uint8_t> data(stride * 2, 0);
	const float red[4] = {1.0f, 0.0f, 0.0f, 1.0f};
	const float green[4] = {0.0f, 1.0f, 0.0f, 1.0f};
	std::memcpy(data.data(), red, sizeof(red));
	std::memcpy(data.data() + stride, green, sizeof(green));

	acm::Buffer uniform = device.createBuffer(data.size(), acm::BufferUsage::Uniform);
	REQUIRE(uniform.valid());
	uniform.write(data.data(), data.size());

	acm::DescriptorSetLayout layout = device.createDescriptorSetLayout({
		{0, acm::DescriptorType::UniformBufferDynamic, acm::ShaderStage::Fragment},
	});
	REQUIRE(layout.valid());
	acm::DescriptorSet descriptors = device.createDescriptorSet(layout);
	REQUIRE(descriptors.valid());
	descriptors.setDynamicBuffer(0, uniform, elementSize);

	acm::Texture color = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::RenderTarget target = device.createRenderTarget(color, acm::RenderTargetConfig{acm::RenderTargetFinish::CopySrc});
	acm::PipelineShaders shaders;
	shaders.vertex = device.createShader(acmtest::fullscreenVertSpirv());
	shaders.fragment = device.createShader(acmtest::colorUniformFragSpirv());
	acm::PipelineConfig config;
	config.descriptorLayout = layout;
	acm::Pipeline pipeline = device.createPipeline(shaders, target, config);
	REQUIRE(pipeline.valid());

	acm::Buffer readback = device.createBuffer(size_t(kSize) * kSize * 4, acm::BufferUsage::TransferDst);
	acm::CommandPool pool = device.createCommandPool();
	acm::CommandBuffer cmd = pool.allocate();
	cmd.begin();
	cmd.beginRendering(target);
	cmd.setViewportAndScissor(extent);
	cmd.bindPipeline(pipeline);
	cmd.bindDescriptorSet(pipeline, descriptors, uint32_t(stride)); // dynamic offset -> element 1
	cmd.draw(3);
	cmd.endRendering();
	cmd.copyTextureToBuffer(color, readback);
	cmd.end();

	REQUIRE_FALSE(device.submitSync(cmd));

	// Element 1 (green) was selected by the dynamic offset, not element 0 (red).
	const auto* px = static_cast<const uint8_t*>(readback.map());
	const size_t center = (size_t(kSize / 2) * kSize + kSize / 2) * 4;
	const uint8_t b = px[center + 0], g = px[center + 1], r = px[center + 2];
	readback.unmap();
	REQUIRE(g > 200);
	REQUIRE(r < 60);
	REQUIRE(b < 60);
}
