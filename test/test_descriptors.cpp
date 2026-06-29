#include "test_spirv.h"
#include "vk_test_helpers.h"

#include <archimedes/archimedes.h>

#include <catch2/catch_all.hpp>
#include <cstdint>
#include <utility>
#include <vector>

// Integration: the extended descriptor model — storage buffers, multi-stage bindings,
// and descriptor arrays. Each is proved by rendered/read-back output. Surface-free.

namespace
{
	// Solid-color pixels for B8G8R8A8_Unorm ([B,G,R,A]).
	std::vector<uint8_t> solid(uint32_t size, uint8_t b, uint8_t g, uint8_t r)
	{
		std::vector<uint8_t> px(size_t(size) * size * 4);
		for (size_t i = 0; i < px.size(); i += 4)
		{
			px[i + 0] = b;
			px[i + 1] = g;
			px[i + 2] = r;
			px[i + 3] = 255;
		}
		return px;
	}

	void submitAndWait(acm::Device& device, acm::CommandBuffer& cmd)
	{
		REQUIRE_FALSE(device.submitSync(cmd));
	}

	constexpr uint32_t kSize = 32;
} // namespace

TEST_CASE("a fragment shader writes a storage buffer", "[acm][gpu]")
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

	const acm::Extent2D extent{kSize, kSize};

	acm::Buffer storage = device.createBuffer(sizeof(uint32_t), acm::BufferUsage::Storage);
	REQUIRE(storage.valid());
	const uint32_t zero = 0;
	storage.write(&zero, sizeof(zero));

	acm::DescriptorSetLayout layout = device.createDescriptorSetLayout({
		{0, acm::DescriptorType::StorageBuffer, acm::ShaderStage::Fragment},
	});
	REQUIRE(layout.valid());
	acm::DescriptorSet descriptors = device.createDescriptorSet(layout);
	REQUIRE(descriptors.valid());
	acm::DescriptorSetLayout retainedLayout = layout;
	layout.reset();
	REQUIRE(retainedLayout.valid());
	layout = std::move(retainedLayout);
	acm::DescriptorSet retainedSet = descriptors;
	descriptors.reset();
	REQUIRE(retainedSet.valid());
	descriptors = std::move(retainedSet);
	descriptors.setBuffer(0, storage);

	acm::Texture color = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::RenderTarget target = device.createRenderTarget(color, acm::RenderTargetFinish::CopySrc);
	acm::PipelineConfig config;
	config.vertex = device.createShader(acmtest::fullscreenVertSpirv());
	config.fragment = device.createShader(acmtest::storageWriteFragSpirv());
	config.target = target;
	config.descriptorLayout = layout;
	acm::Pipeline pipeline = device.createPipeline(config);
	REQUIRE(pipeline.valid());

	acm::CommandPool pool = device.createCommandPool();
	acm::CommandBuffer cmd = pool.allocate();
	cmd.begin();
	cmd.beginRendering(target);
	cmd.setViewportAndScissor(extent);
	cmd.bindPipeline(pipeline);
	cmd.bindDescriptorSet(pipeline, descriptors);
	cmd.draw(3);
	cmd.endRendering();
	cmd.end();
	submitAndWait(device, cmd);

	// The shader wrote 42 into the storage buffer; the CPU reads it back.
	const auto* value = static_cast<const uint32_t*>(storage.map());
	REQUIRE(value != nullptr);
	REQUIRE(*value == 42u);
	storage.unmap();
}

TEST_CASE("one uniform binding feeds both shader stages", "[acm][gpu]")
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

	const acm::Extent2D extent{kSize, kSize};

	// vec4: xyz = color (green), w = scale (1 — triangle covers the center).
	const float data[4] = {0.0f, 1.0f, 0.0f, 1.0f};
	acm::Buffer uniform = device.createBuffer(sizeof(data), acm::BufferUsage::Uniform);
	uniform.write(data, sizeof(data));

	// One binding, visible to BOTH stages: the vertex shader reads .w, the fragment .xyz.
	acm::DescriptorSetLayout layout = device.createDescriptorSetLayout({
		{0, acm::DescriptorType::UniformBuffer, acm::ShaderStage::Vertex | acm::ShaderStage::Fragment},
	});
	REQUIRE(layout.valid());
	acm::DescriptorSet descriptors = device.createDescriptorSet(layout);
	descriptors.setBuffer(0, uniform);

	acm::Texture color = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::RenderTarget target = device.createRenderTarget(color, acm::RenderTargetFinish::CopySrc);
	acm::PipelineConfig config;
	config.vertex = device.createShader(acmtest::multiStageVertSpirv());
	config.fragment = device.createShader(acmtest::multiStageFragSpirv());
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
	cmd.copyTextureToBuffer(color, readback);
	cmd.end();
	submitAndWait(device, cmd);

	// Center is green: the fragment read the color, and the vertex read the scale from
	// the same binding (a wrong stage mask would be a validation error, not green).
	const auto* px = static_cast<const uint8_t*>(readback.map());
	const size_t center = (size_t(kSize / 2) * kSize + kSize / 2) * 4;
	const uint8_t b = px[center + 0], g = px[center + 1], r = px[center + 2];
	readback.unmap();
	REQUIRE(g > 200);
	REQUIRE(r < 60);
	REQUIRE(b < 60);
}

TEST_CASE("a descriptor array selects the right texture", "[acm][gpu]")
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

	const acm::Extent2D extent{kSize, kSize};

	acm::Texture texRed = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::Texture texGreen = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	const auto red = solid(kSize, 0, 0, 255);
	const auto green = solid(kSize, 0, 255, 0);
	texRed.upload(red.data(), red.size());
	texGreen.upload(green.data(), green.size());
	acm::Sampler sampler = device.createSampler();

	// A 2-element sampler array; the shader samples element 1.
	acm::DescriptorSetLayout layout = device.createDescriptorSetLayout({
		{0, acm::DescriptorType::CombinedImageSampler, acm::ShaderStage::Fragment, 2},
	});
	REQUIRE(layout.valid());
	acm::DescriptorSet descriptors = device.createDescriptorSet(layout);
	descriptors.setTexture(0, texRed, sampler, 0);	 // element 0 = red
	descriptors.setTexture(0, texGreen, sampler, 1); // element 1 = green

	acm::Texture out = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::RenderTarget target = device.createRenderTarget(out, acm::RenderTargetFinish::CopySrc);
	acm::PipelineConfig config;
	config.vertex = device.createShader(acmtest::fullscreenVertSpirv());
	config.fragment = device.createShader(acmtest::samplerArrayFragSpirv());
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
	submitAndWait(device, cmd);

	// The shader sampled array element 1 (green), not element 0 (red).
	const auto* px = static_cast<const uint8_t*>(readback.map());
	const size_t center = (size_t(kSize / 2) * kSize + kSize / 2) * 4;
	const uint8_t b = px[center + 0], g = px[center + 1], r = px[center + 2];
	readback.unmap();
	REQUIRE(g > 200);
	REQUIRE(r < 60);
	REQUIRE(b < 60);
}
