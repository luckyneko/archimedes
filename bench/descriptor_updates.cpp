/*
 *  Created by LuckyNeko on 02/07/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "bench_helpers.h"

#include <catch2/catch_all.hpp>

TEST_CASE("descriptor_updates", "[bench][fast][gpu]")
{
	acmbench::DeviceStack stack;
	if (!acmbench::buildGraphicsDevice(stack, "acm-descriptor-benchmark"))
		return;

	acm::Buffer uniform = stack.device.createBuffer(256, acm::BufferUsage::Uniform);
	acm::DescriptorSetLayout uniformLayout = stack.device.createDescriptorSetLayout({
		{0, acm::DescriptorType::UniformBuffer, acm::ShaderStage::Vertex},
	});
	acm::DescriptorSet uniformSet = stack.device.createDescriptorSet(uniformLayout);
	REQUIRE(uniform.valid());
	REQUIRE(uniformLayout.valid());
	REQUIRE(uniformSet.valid());

	BENCHMARK("acm(uniform-buffer)")
	{
		uniformSet.setBuffer(0, uniform);
		return uniformSet.valid();
	};

	acm::Buffer dynamicUniform = stack.device.createBuffer(512, acm::BufferUsage::Uniform);
	acm::DescriptorSetLayout dynamicLayout = stack.device.createDescriptorSetLayout({
		{0, acm::DescriptorType::UniformBufferDynamic, acm::ShaderStage::Vertex},
	});
	acm::DescriptorSet dynamicSet = stack.device.createDescriptorSet(dynamicLayout);
	REQUIRE(dynamicUniform.valid());
	REQUIRE(dynamicLayout.valid());
	REQUIRE(dynamicSet.valid());

	BENCHMARK("acm(dynamic-uniform-buffer)")
	{
		dynamicSet.setDynamicBuffer(0, dynamicUniform, 256);
		return dynamicSet.valid();
	};

	acm::Texture texture = stack.device.createTexture(acm::Format::B8G8R8A8_Unorm, acm::Extent2D{64, 64});
	acm::Sampler sampler = stack.device.createSampler();
	acm::DescriptorSetLayout textureLayout = stack.device.createDescriptorSetLayout(1);
	acm::DescriptorSet textureSet = stack.device.createDescriptorSet(textureLayout);
	REQUIRE(texture.valid());
	REQUIRE(sampler.valid());
	REQUIRE(textureLayout.valid());
	REQUIRE(textureSet.valid());

	BENCHMARK("acm(combined-image-sampler)")
	{
		textureSet.setTexture(0, texture, sampler);
		return textureSet.valid();
	};
}
