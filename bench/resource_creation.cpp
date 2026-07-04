/*
 *  Created by LuckyNeko on 02/07/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "bench_helpers.h"
#include "test_spirv.h"

#include <catch2/catch_all.hpp>
#include <cstdint>
#include <vector>

TEST_CASE("resource_creation", "[bench][fast][gpu]")
{
	acmbench::DeviceStack stack;
	if (!acmbench::buildGraphicsDevice(stack, "acm-resource-benchmark"))
		return;

	const acm::Extent2D extent{64, 64};
	const std::vector<char> vertexSpirv = acmtest::triangleVertSpirv();
	const std::vector<char> fragmentSpirv = acmtest::triangleFragSpirv();

	auto cleanup = [&]
	{
		stack.device.waitIdle();
	};

	BENCHMARK("acm(buffer-uniform-4k-x64)")
	{
		std::vector<acm::Buffer> buffers;
		buffers.reserve(64);
		for (uint32_t i = 0; i < 64; ++i)
			buffers.push_back(stack.device.createBuffer(4096, acm::BufferUsage::Uniform));

		bool valid = true;
		for (const acm::Buffer& buffer : buffers)
			valid = valid && buffer.valid();
		for (acm::Buffer& buffer : buffers)
			buffer.reset();
		cleanup();
		return valid;
	};

	BENCHMARK("acm(texture-bgra8-64-x16)")
	{
		std::vector<acm::Texture> textures;
		textures.reserve(16);
		for (uint32_t i = 0; i < 16; ++i)
			textures.push_back(stack.device.createTexture(acm::Format::B8G8R8A8_Unorm, extent));

		bool valid = true;
		for (const acm::Texture& texture : textures)
			valid = valid && texture.valid();
		for (acm::Texture& texture : textures)
			texture.reset();
		cleanup();
		return valid;
	};

	acm::Texture targetTexture = stack.device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	REQUIRE(targetTexture.valid());
	BENCHMARK("acm(render-target-bgra8-64-x16)")
	{
		std::vector<acm::RenderTarget> targets;
		targets.reserve(16);
		for (uint32_t i = 0; i < 16; ++i)
			targets.push_back(stack.device.createRenderTarget(targetTexture, acm::RenderTargetConfig{acm::RenderTargetFinish::CopySrc}));

		bool valid = true;
		for (const acm::RenderTarget& target : targets)
			valid = valid && target.valid();
		for (acm::RenderTarget& target : targets)
			target.reset();
		cleanup();
		return valid;
	};

	BENCHMARK("acm(shader-triangle-vert-x16)")
	{
		std::vector<acm::Shader> shaders;
		shaders.reserve(16);
		for (uint32_t i = 0; i < 16; ++i)
			shaders.push_back(stack.device.createShader(vertexSpirv));

		bool valid = true;
		for (const acm::Shader& shader : shaders)
			valid = valid && shader.valid();
		for (acm::Shader& shader : shaders)
			shader.reset();
		cleanup();
		return valid;
	};

	acm::RenderTarget target = stack.device.createRenderTarget(targetTexture, acm::RenderTargetConfig{acm::RenderTargetFinish::CopySrc});
	acm::Shader vertex = stack.device.createShader(vertexSpirv);
	acm::Shader fragment = stack.device.createShader(fragmentSpirv);
	REQUIRE(target.valid());
	REQUIRE(vertex.valid());
	REQUIRE(fragment.valid());
	BENCHMARK("acm(pipeline-triangle-x4)")
	{
		std::vector<acm::Pipeline> pipelines;
		pipelines.reserve(4);
		for (uint32_t i = 0; i < 4; ++i)
			pipelines.push_back(stack.device.createPipeline(vertex, fragment, target));

		bool valid = true;
		for (const acm::Pipeline& pipeline : pipelines)
			valid = valid && pipeline.valid();
		for (acm::Pipeline& pipeline : pipelines)
			pipeline.reset();
		cleanup();
		return valid;
	};
}
