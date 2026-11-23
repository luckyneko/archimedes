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
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// Integration: exercises the command pool/buffer handles and the acm::Renderer
// frame loop (acquire -> record -> submit -> present) over a headless swapchain.
// SKIPs without a live driver / headless surface / swapchain support.

TEST_CASE("CommandPool allocates a recordable buffer", "[acm][gpu]")
{
	acm::Instance instance = acmtest::createInstance();
	acmtest::requireInstance(instance);

	uint32_t queueIndex = 0;
	const acm::DeviceInfo* gpu = acmtest::selectGraphicsDevice(instance, queueIndex);

	acm::Device device = instance.createDevice(*gpu, queueIndex);
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
	acm::Pipeline pipeline = s.device.createPipeline(acm::PipelineShaders{vert, frag}, s.swapChain.renderTarget(0));
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

TEST_CASE("Renderer returns command recording errors and keeps rendering", "[acm][gpu]")
{
	// Everything the render thread touches, on the heap so a stuck thread can be left with it.
	struct Scene
	{
		acmtest::HeadlessStack s;
		acm::Texture texture;
		acm::RenderTarget depthTarget;
		acm::Pipeline depthPipeline;
		acm::Pipeline pipeline;
		acm::Renderer renderer;
		std::string incompatibleError;
		std::string drawError;
		std::string recordError;
		std::string prePassError;
		bool recordRanAfterPrePassError{false};
		std::vector<std::string> laterErrors;
		int laterRecorded{0};
	};
	auto scene = std::make_unique<Scene>();
	acmtest::HeadlessStack& s = scene->s;
	if (!acmtest::buildHeadlessStack(s))
		return;

	// A pipeline built for a depth target: binding it in the depth-less swapchain pass fails.
	const acm::Extent2D extent{64, 64};
	scene->texture = s.device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	scene->depthTarget = s.device.createRenderTarget(scene->texture, acm::RenderTargetConfig{acm::RenderTargetFinish::CopySrc, true});
	REQUIRE(scene->depthTarget.valid());
	acm::PipelineShaders shaders;
	shaders.vertex = s.device.createShader(acmtest::triangleVertSpirv());
	shaders.fragment = s.device.createShader(acmtest::triangleFragSpirv());
	scene->depthPipeline = s.device.createPipeline(shaders, scene->depthTarget);
	scene->pipeline = s.device.createPipeline(shaders, s.swapChain.renderTarget(0));
	REQUIRE(scene->depthPipeline.valid());
	REQUIRE(scene->pipeline.valid());

	scene->renderer = s.device.createRenderer(s.swapChain);
	REQUIRE(scene->renderer.valid());

	// A failed frame must still be submitted and presented: otherwise the next acquire reuses
	// a signalled semaphore, and once every image is held acquire blocks forever. Enough good
	// frames follow to cycle every swapchain image and frame slot twice.
	const int laterFrames = int(2 * s.swapChain.renderTargetCount()) + 1;
	std::promise<void> finished;
	std::future<void> done = finished.get_future();
	Scene* shared = scene.get();
	std::thread worker([shared, laterFrames, finished = std::move(finished)]() mutable
					   {
		acm::Renderer& renderer = shared->renderer;
		shared->incompatibleError = renderer.render([shared](acm::CommandBuffer& cmd, uint32_t)
													{
			cmd.bindPipeline(shared->depthPipeline);
			cmd.draw(3); }).message();
		shared->recordError = renderer.render([shared](acm::CommandBuffer& cmd, uint32_t)
											  { shared->drawError = cmd.draw(3).message(); }).message();
		shared->prePassError = renderer.render([](acm::CommandBuffer& cmd, uint32_t)
											   { cmd.draw(3); },
											   [shared](acm::CommandBuffer&, uint32_t)
											   { shared->recordRanAfterPrePassError = true; }).message();
		for (int frame = 0; frame < laterFrames; ++frame)
		{
			const acm::Error error = renderer.render([shared](acm::CommandBuffer& cmd, uint32_t)
													 {
				cmd.bindPipeline(shared->pipeline);
				cmd.draw(3);
				++shared->laterRecorded; });
			if (error)
				shared->laterErrors.push_back(error.message());
		}
		finished.set_value(); });

	if (done.wait_for(std::chrono::seconds(60)) != std::future_status::ready)
	{
		// The render thread is stuck in the driver: leave it everything it uses rather than
		// destroy that under it, and fail instead of hanging the suite.
		worker.detach();
		static_cast<void>(scene.release());
		FAIL("render() did not return within 60 s after a frame with a recording error");
	}
	worker.join();

	CHECK_FALSE(shared->incompatibleError.empty());
	CHECK_FALSE(shared->drawError.empty());
	CHECK(shared->recordError == shared->drawError);
	CHECK_FALSE(shared->prePassError.empty());
	CHECK_FALSE(shared->recordRanAfterPrePassError);
	for (const std::string& error : shared->laterErrors)
		UNSCOPED_INFO("later frame error: " << error);
	CHECK(shared->laterErrors.empty());
	CHECK(shared->laterRecorded == laterFrames);
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

	acm::Pipeline graphics = s.device.createPipeline(
		acm::PipelineShaders{s.device.createShader(acmtest::triangleVertSpirv()),
							 s.device.createShader(acmtest::triangleFragSpirv())},
		s.swapChain.renderTarget(0));
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
