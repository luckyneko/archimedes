/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "test_spirv.h"
#include "test_spirv_api_errors.h"
#include "vk_test_helpers.h"

#include <archimedes/acmVulkanInterop.h>
#include <archimedes/archimedes.h>

#include <catch2/catch_all.hpp>
#include <cstdint>
#include <utility>
#include <vector>

// Integration: the extended descriptor model — storage buffers, multi-stage bindings,
// descriptor arrays, and sampled images. Each is proved by rendered/read-back output.
// Surface-free.

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
	acm::Instance instance = acmtest::createInstance();
	acmtest::requireInstance(instance);
	uint32_t queueIndex = 0;
	const acm::DeviceInfo* gpu = acmtest::selectGraphicsDevice(instance, queueIndex);
	acm::Device device = instance.createDevice(*gpu, queueIndex);
	REQUIRE(device.valid());
	// A fragment-stage store is only valid Vulkan with this feature enabled.
	if (!device.enabledFeatures().fragmentStoresAndAtomics)
		SKIP("device does not support fragmentStoresAndAtomics");

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
	acm::RenderTarget target = device.createRenderTarget(color, acm::RenderTargetConfig{acm::RenderTargetFinish::CopySrc});
	acm::PipelineShaders shaders;
	shaders.vertex = device.createShader(acmtest::fullscreenVertSpirv());
	shaders.fragment = device.createShader(acmtest::storageWriteFragSpirv());
	acm::PipelineConfig config;
	config.descriptorLayout = layout;
	acm::Pipeline pipeline = device.createPipeline(shaders, target, config);
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
	acm::Instance instance = acmtest::createInstance();
	acmtest::requireInstance(instance);
	uint32_t queueIndex = 0;
	const acm::DeviceInfo* gpu = acmtest::selectGraphicsDevice(instance, queueIndex);
	acm::Device device = instance.createDevice(*gpu, queueIndex);
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
	acm::RenderTarget target = device.createRenderTarget(color, acm::RenderTargetConfig{acm::RenderTargetFinish::CopySrc});
	acm::PipelineShaders shaders;
	shaders.vertex = device.createShader(acmtest::multiStageVertSpirv());
	shaders.fragment = device.createShader(acmtest::multiStageFragSpirv());
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
	acm::Instance instance = acmtest::createInstance();
	acmtest::requireInstance(instance);
	uint32_t queueIndex = 0;
	const acm::DeviceInfo* gpu = acmtest::selectGraphicsDevice(instance, queueIndex);
	acm::Device device = instance.createDevice(*gpu, queueIndex);
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
	acm::RenderTarget target = device.createRenderTarget(out, acm::RenderTargetConfig{acm::RenderTargetFinish::CopySrc});
	acm::PipelineShaders shaders;
	shaders.vertex = device.createShader(acmtest::fullscreenVertSpirv());
	shaders.fragment = device.createShader(acmtest::samplerArrayFragSpirv());
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

TEST_CASE("a sampled image is read without a sampler", "[acm][gpu]")
{
	acm::Instance instance = acmtest::createInstance();
	acmtest::requireInstance(instance);
	uint32_t queueIndex = 0;
	const acm::DeviceInfo* gpu = acmtest::selectGraphicsDevice(instance, queueIndex);
	acm::Device device = instance.createDevice(*gpu, queueIndex);
	REQUIRE(device.valid());

	const acm::Extent2D extent{kSize, kSize};

	// Left half red, right half green: the shader reads the texel at its own pixel, so the
	// output reproduces the split only if the descriptor really points at this image.
	std::vector<uint8_t> pixels = solid(kSize, 0, 0, 255);
	for (uint32_t y = 0; y < kSize; ++y)
	{
		for (uint32_t x = kSize / 2; x < kSize; ++x)
		{
			const size_t at = (size_t(y) * kSize + x) * 4;
			pixels[at + 1] = 255; // G
			pixels[at + 2] = 0;	  // R
		}
	}
	acm::Texture source = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	source.upload(pixels.data(), pixels.size()); // leaves it SHADER_READ_ONLY

	acm::DescriptorSetLayout layout = device.createDescriptorSetLayout({
		{0, acm::DescriptorType::SampledImage, acm::ShaderStage::Fragment},
	});
	REQUIRE(layout.valid());
	acm::DescriptorSet descriptors = device.createDescriptorSet(layout);
	REQUIRE(descriptors.valid());
	descriptors.setSampledImage(0, source);

	// The raw handle an external renderer binds (ImGui's ImTextureID is one); a stale or empty
	// wrapper answers null rather than a dangling handle.
	REQUIRE(acm::interop::descriptorSet(descriptors) != VK_NULL_HANDLE);
	REQUIRE(acm::interop::descriptorSet(acm::DescriptorSet{}) == VK_NULL_HANDLE);

	acm::Texture out = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::RenderTarget target = device.createRenderTarget(out, acm::RenderTargetConfig{acm::RenderTargetFinish::CopySrc});
	acm::PipelineShaders shaders;
	shaders.vertex = device.createShader(acmtest::fullscreenVertSpirv());
	shaders.fragment = device.createShader(acmtest::sampledImageFragSpirv());
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
	cmd.bindDescriptorSet(pipeline, descriptors);
	cmd.draw(3);
	cmd.endRendering();
	cmd.copyTextureToBuffer(out, readback);
	cmd.end();
	submitAndWait(device, cmd);

	const auto* px = static_cast<const uint8_t*>(readback.map());
	const size_t row = size_t(kSize / 2) * kSize;
	const size_t left = (row + kSize / 4) * 4;
	const size_t right = (row + 3 * kSize / 4) * 4;
	const uint8_t leftR = px[left + 2], leftG = px[left + 1];
	const uint8_t rightR = px[right + 2], rightG = px[right + 1];
	readback.unmap();
	REQUIRE(leftR == 255);
	REQUIRE(leftG == 0);
	REQUIRE(rightR == 0);
	REQUIRE(rightG == 255);
}

TEST_CASE("descriptor writes reject bad bindings, elements, types and resources", "[acm][gpu]")
{
	acm::Instance instance = acmtest::createInstance();
	acmtest::requireInstance(instance);
	uint32_t queueIndex = 0;
	const acm::DeviceInfo* gpu = acmtest::selectGraphicsDevice(instance, queueIndex);
	acm::Device device = instance.createDevice(*gpu, queueIndex);
	acm::Device other = instance.createDevice(*gpu, queueIndex);
	REQUIRE(device.valid());
	REQUIRE(other.valid());

	const acm::Extent2D extent{kSize, kSize};
	acm::DescriptorSetLayout layout = device.createDescriptorSetLayout({
		{0, acm::DescriptorType::CombinedImageSampler, acm::ShaderStage::Fragment, 2},
		{1, acm::DescriptorType::UniformBuffer, acm::ShaderStage::Fragment},
		{2, acm::DescriptorType::UniformBufferDynamic, acm::ShaderStage::Fragment},
		{3, acm::DescriptorType::StorageImage, acm::ShaderStage::Compute},
		{4, acm::DescriptorType::SampledImage, acm::ShaderStage::Fragment},
		{5, acm::DescriptorType::StorageBuffer, acm::ShaderStage::Fragment},
	});
	REQUIRE(layout.valid());
	acm::DescriptorSet set = device.createDescriptorSet(layout);
	REQUIRE(set.valid());

	acm::TextureConfig storageConfig;
	storageConfig.storage = true;
	acm::Texture texture = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::Texture storageTexture = device.createTexture(acm::Format::R8G8B8A8_Unorm, extent, storageConfig);
	acm::Texture depth = device.createTexture(acm::Format::D32_Sfloat, extent);
	acm::Sampler sampler = device.createSampler();
	acm::Buffer uniform = device.createBuffer(256, acm::BufferUsage::Uniform);
	acm::Buffer storage = device.createBuffer(256, acm::BufferUsage::Storage);
	acm::Texture otherTexture = other.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::Sampler otherSampler = other.createSampler();
	acm::Buffer otherBuffer = other.createBuffer(256, acm::BufferUsage::Uniform);
	REQUIRE(texture.valid());
	REQUIRE(storageTexture.valid());
	REQUIRE(depth.valid());
	REQUIRE(sampler.valid());
	REQUIRE(uniform.valid());
	REQUIRE(storage.valid());
	REQUIRE(otherTexture.valid());
	REQUIRE(otherSampler.valid());
	REQUIRE(otherBuffer.valid());

	auto requireError = [](const acm::Error& error, const char* expected)
	{
		REQUIRE(error);
		CHECK_THAT(error.message(), Catch::Matchers::ContainsSubstring(expected));
	};

	// A binding missing from the layout, an element past the array, and calls that don't
	// match the binding's type.
	requireError(set.setTexture(9, texture, sampler), "binding 9");
	requireError(set.setBuffer(9, uniform), "binding 9");
	requireError(set.setTexture(0, texture, sampler, 2), "array element 2");
	requireError(set.setBuffer(1, uniform, 1), "array element 1");
	requireError(set.setTexture(1, texture, sampler), "UniformBuffer");
	requireError(set.setBuffer(0, uniform), "CombinedImageSampler");
	requireError(set.setBuffer(2, uniform), "UniformBufferDynamic");
	requireError(set.setDynamicBuffer(1, uniform, 16), "UniformBuffer");
	requireError(set.setStorageImage(4, storageTexture), "SampledImage");
	requireError(set.setSampledImage(3, texture), "StorageImage");

	// Invalid, unusable and other-device resources.
	requireError(set.setTexture(0, acm::Texture{}, sampler), "texture");
	requireError(set.setTexture(0, texture, acm::Sampler{}), "sampler");
	requireError(set.setBuffer(1, acm::Buffer{}), "buffer");
	requireError(set.setDynamicBuffer(2, acm::Buffer{}, 16), "buffer");
	requireError(set.setStorageImage(3, acm::Texture{}), "texture");
	requireError(set.setSampledImage(4, acm::Texture{}), "texture");
	requireError(set.setTexture(0, depth, sampler), "depth");
	requireError(set.setSampledImage(4, depth), "depth");
	requireError(set.setDynamicBuffer(2, uniform, 0), "size");
	requireError(set.setDynamicBuffer(2, uniform, 257), "size");
	requireError(set.setTexture(0, otherTexture, sampler), "another device");
	requireError(set.setTexture(0, texture, otherSampler), "another device");
	requireError(set.setBuffer(1, otherBuffer), "another device");
	requireError(acm::DescriptorSet{}.setBuffer(1, uniform), "descriptor set");

	// A resource whose usage does not fit the descriptor, judged by the Vulkan usage each
	// BufferUsage maps to: uniform and dynamic uniform descriptors need a Uniform buffer,
	// a storage buffer descriptor a Storage one, and a storage image a texture created with
	// TextureConfig::storage.
	acm::Buffer vertex = device.createBuffer(256, acm::BufferUsage::Vertex);
	REQUIRE(vertex.valid());
	requireError(set.setBuffer(1, storage), "BufferUsage::Uniform");
	requireError(set.setBuffer(1, vertex), "BufferUsage::Vertex");
	requireError(set.setBuffer(5, uniform), "BufferUsage::Storage");
	requireError(set.setDynamicBuffer(2, storage, 16), "BufferUsage::Uniform");
	requireError(set.setStorageImage(3, texture), "TextureConfig::storage");

	// Every call with a matching binding, element and resource succeeds.
	REQUIRE_FALSE(set.setTexture(0, texture, sampler, 0));
	REQUIRE_FALSE(set.setTexture(0, texture, sampler, 1));
	REQUIRE_FALSE(set.setBuffer(1, uniform));
	REQUIRE_FALSE(set.setDynamicBuffer(2, uniform, 16));
	REQUIRE_FALSE(set.setStorageImage(3, storageTexture));
	REQUIRE_FALSE(set.setSampledImage(4, texture));
	REQUIRE_FALSE(set.setBuffer(5, storage));
}

TEST_CASE("a descriptor set keeps the resources written into it alive", "[acm][gpu]")
{
	acm::Instance instance = acmtest::createInstance();
	acmtest::requireInstance(instance);
	uint32_t queueIndex = 0;
	const acm::DeviceInfo* gpu = acmtest::selectGraphicsDevice(instance, queueIndex);
	acm::Device device = instance.createDevice(*gpu, queueIndex);
	REQUIRE(device.valid());

	const acm::Extent2D extent{kSize, kSize};
	acm::DescriptorSetLayout layout = device.createDescriptorSetLayout({
		{0, acm::DescriptorType::CombinedImageSampler, acm::ShaderStage::Fragment},
		{1, acm::DescriptorType::UniformBuffer, acm::ShaderStage::Fragment},
	});
	REQUIRE(layout.valid());
	acm::DescriptorSet set = device.createDescriptorSet(layout);
	REQUIRE(set.valid());

	// Write a green texture, its sampler and a uniform that adds blue, then drop every
	// wrapper the test holds for them: only the set still references them.
	{
		acm::Texture texture = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
		const auto green = solid(kSize, 0, 255, 0);
		REQUIRE_FALSE(texture.upload(green.data(), green.size()));
		acm::Sampler sampler = device.createSampler();
		const float addBlue[4] = {0.0f, 0.0f, 1.0f, 0.0f};
		acm::Buffer tint = device.createBuffer(sizeof(addBlue), acm::BufferUsage::Uniform);
		REQUIRE_FALSE(tint.write(addBlue, sizeof(addBlue)));
		set.setTexture(0, texture, sampler);
		set.setBuffer(1, tint);
	}

	// A completed submission collects garbage, destroying anything nothing references;
	// decoys of the same shape then reuse whatever memory that freed.
	REQUIRE_FALSE(device.submitSync([](acm::CommandBuffer&) {}));
	acm::Texture decoyTexture = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	const auto red = solid(kSize, 0, 0, 255);
	REQUIRE_FALSE(decoyTexture.upload(red.data(), red.size()));
	const float addRed[4] = {1.0f, 0.0f, 0.0f, 0.0f};
	acm::Buffer decoyTint = device.createBuffer(sizeof(addRed), acm::BufferUsage::Uniform);
	REQUIRE_FALSE(decoyTint.write(addRed, sizeof(addRed)));

	acm::Texture out = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::RenderTarget target = device.createRenderTarget(out, acm::RenderTargetConfig{acm::RenderTargetFinish::CopySrc});
	acm::PipelineShaders shaders;
	shaders.vertex = device.createShader(acmtest::fullscreenVertSpirv());
	shaders.fragment = device.createShader(acmtest::textureTintFragSpirv());
	acm::PipelineConfig config;
	config.descriptorLayout = layout;
	acm::Pipeline pipeline = device.createPipeline(shaders, target, config);
	REQUIRE(pipeline.valid());

	acm::Buffer readback = device.createBuffer(size_t(kSize) * kSize * 4, acm::BufferUsage::TransferDst);
	acm::CommandPool pool = device.createCommandPool();
	acm::CommandBuffer cmd = pool.allocate();
	REQUIRE_FALSE(cmd.begin());
	REQUIRE_FALSE(cmd.beginRendering(target));
	cmd.setViewportAndScissor(extent);
	REQUIRE_FALSE(cmd.bindPipeline(pipeline));
	cmd.bindDescriptorSet(pipeline, set);
	REQUIRE_FALSE(cmd.draw(3));
	cmd.endRendering();
	cmd.copyTextureToBuffer(out, readback);
	REQUIRE_FALSE(cmd.error());
	REQUIRE_FALSE(cmd.end());
	submitAndWait(device, cmd);

	// Green from the retained texture plus blue from the retained uniform: cyan, with no
	// red from the decoys.
	const auto* px = static_cast<const uint8_t*>(readback.map());
	const size_t center = (size_t(kSize / 2) * kSize + kSize / 2) * 4;
	const uint8_t b = px[center + 0], g = px[center + 1], r = px[center + 2];
	readback.unmap();
	REQUIRE(g > 200);
	REQUIRE(b > 200);
	REQUIRE(r < 60);
}
