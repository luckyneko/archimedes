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
#include <vector>

// Integration: exercises the command pool/buffer handles and the acm::Renderer
// frame loop (acquire -> record -> submit -> present) over a headless swapchain.
// SKIPs without a live driver / headless surface / swapchain support.

TEST_CASE("CommandPool allocates a recordable buffer", "[acm][gpu]")
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

	acm::CommandPool pool = device.createCommandPool();
	REQUIRE(pool.valid());

	acm::CommandBuffer cmd = pool.allocate();
	REQUIRE(cmd.valid());

	// A bare begin/end (no rendering scope) is a valid recording — must not crash.
	cmd.begin();
	cmd.end();

	// The buffer keeps its pool alive: dropping the pool handle leaves it usable.
	pool.reset();
	REQUIRE(cmd.valid());
	REQUIRE_FALSE(cmd.begin());
	REQUIRE_FALSE(cmd.end());
}

TEST_CASE("Renderer drives frames and records draws", "[acm][gpu]")
{
	acmtest::HeadlessStack s;
	if (!acmtest::buildHeadlessStack(s))
		return;

	acm::Shader vert = s.device.createShader(acmtest::triangleVertSpirv());
	acm::Shader frag = s.device.createShader(acmtest::triangleFragSpirv());
	acm::Pipeline pipeline = s.device.createPipeline(vert, frag, s.swapChain.getRenderTarget(0));
	REQUIRE(pipeline.valid());

	acm::Renderer renderer = s.device.createRenderer(s.swapChain);
	REQUIRE(renderer.valid());
	acm::Renderer retained = renderer;
	renderer.reset();
	REQUIRE(retained.valid());
	renderer = std::move(retained);
	REQUIRE(renderer.valid());
	REQUIRE_FALSE(retained.valid());

	// Run more frames than MaxFramesInFlight (2) so the fence/slot wrap is exercised.
	// The callback must fire once per acquired frame, and the frame index it receives
	// must stay within the ring.
	int recorded = 0;
	for (int i = 0; i < 5; ++i)
	{
		renderer.render([&](acm::CommandBuffer& cmd, uint32_t frameIndex)
						{
			REQUIRE(frameIndex < acm::Renderer::MaxFramesInFlight);
			cmd.bindPipeline(pipeline);
			cmd.draw(3);
			++recorded; });
	}
	REQUIRE(recorded == 5);
}

TEST_CASE("Renderer runs a compute pre-pass before the draw", "[acm][gpu]")
{
	acmtest::HeadlessStack s;
	if (!acmtest::buildHeadlessStack(s))
		return;

	// A storage buffer the pre-pass compute fills (values[i] = i*2+1), read back after.
	constexpr uint32_t kCount = 256;
	acm::Buffer storage = s.device.createBuffer(kCount * sizeof(uint32_t), acm::BufferUsage::Storage);
	REQUIRE(storage.valid());
	std::vector<uint32_t> zeros(kCount, 0);
	storage.write(zeros.data(), zeros.size() * sizeof(uint32_t));

	acm::DescriptorSetLayout computeLayout = s.device.createDescriptorSetLayout({
		{0, acm::DescriptorType::StorageBuffer, acm::ShaderStage::Compute},
	});
	acm::DescriptorSet computeSet = s.device.createDescriptorSet(computeLayout);
	computeSet.setBuffer(0, storage);
	acm::ComputePipeline compute = s.device.createComputePipeline(s.device.createShader(acmtest::computeFillSpirv()), computeLayout);
	REQUIRE(compute.valid());

	acm::Pipeline graphics = s.device.createPipeline(s.device.createShader(acmtest::triangleVertSpirv()), s.device.createShader(acmtest::triangleFragSpirv()), s.swapChain.getRenderTarget(0));
	REQUIRE(graphics.valid());

	acm::Renderer renderer = s.device.createRenderer(s.swapChain);
	REQUIRE(renderer.valid());

	for (int i = 0; i < 3; ++i)
	{
		renderer.render(
			[&](acm::CommandBuffer& cmd, uint32_t) // pre-pass: compute, outside dynamic rendering
			{
				cmd.bindComputePipeline(compute);
				cmd.bindComputeDescriptorSet(compute, computeSet);
				cmd.dispatch(kCount / 64, 1, 1);
				cmd.bufferBarrier(storage, acm::ShaderStage::Compute, acm::ShaderStage::Vertex);
			},
			[&](acm::CommandBuffer& cmd, uint32_t) // draws, inside dynamic rendering
			{
				cmd.bindPipeline(graphics);
				cmd.draw(3);
			});
	}

	// The pre-pass compute ran in-frame: the storage buffer holds its output.
	s.device.waitIdle();
	const auto* values = static_cast<const uint32_t*>(storage.map());
	REQUIRE(values != nullptr);
	bool allCorrect = true;
	for (uint32_t i = 0; i < kCount; ++i)
		if (values[i] != i * 2u + 1u)
			allCorrect = false;
	storage.unmap();
	REQUIRE(allCorrect);
}
