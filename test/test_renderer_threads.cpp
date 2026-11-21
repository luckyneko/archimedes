/*
 *  Created by LuckyNeko on 22/11/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "test_spirv.h"
#include "vk_test_helpers.h"

#include <archimedes/archimedes.h>

#include <catch2/catch_all.hpp>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

// Integration: two Renderers share one Device, each over its own headless swapchain and
// driven from its own thread, as the testbed's ripple-mesh drives two windows. Every few
// frames each thread recreates its swapchain, a device-idle wait while the other thread
// keeps submitting, and every frame draws from a vertex + index buffer created for it and
// uploaded through a staged one-shot submission, then dropped once render() has submitted
// the frame that reads them. Every frame and recreate must succeed. The run is a fixed
// number of frames per thread, so it ends deterministically. SKIPs where this run's device
// kind cannot present to two headless surfaces.

namespace
{
	struct Vertex
	{
		float pos[2];
		float color[3];
	};

	constexpr acm::Extent2D kExtent{256, 256};
	constexpr int kFrames = 30;
	constexpr int kRecreateEvery = 7;

	// One window's device state, used only by the thread that renders it. Errors are
	// collected rather than asserted because Catch2 assertions are not thread-safe.
	struct View
	{
		acm::SwapChain swapChain;
		acm::Renderer renderer;
		acm::Pipeline pipeline;
		std::vector<std::string> errors;
		int recorded{0};
		int recreated{0};

		void run(acm::Device& device)
		{
			const Vertex vertices[3] = {
				{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
				{{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
				{{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
			};
			const uint32_t indices[3] = {0, 1, 2};
			for (int frame = 0; frame < kFrames; ++frame)
			{
				if (frame % kRecreateEvery == kRecreateEvery - 1)
				{
					if (swapChain.recreate())
						++recreated;
					else
						errors.push_back("swapchain recreate failed");
				}

				acm::Buffer vertexBuffer = device.createBuffer(sizeof(vertices), acm::BufferUsage::Vertex);
				acm::Buffer indexBuffer = device.createBuffer(sizeof(indices), acm::BufferUsage::Index);
				if (acm::Error error = vertexBuffer.write(vertices, sizeof(vertices)))
					errors.push_back(error.message());
				if (acm::Error error = indexBuffer.write(indices, sizeof(indices)))
					errors.push_back(error.message());
				const acm::Error error = renderer.render([&](acm::CommandBuffer& cmd, uint32_t)
														 {
					cmd.bindPipeline(pipeline);
					cmd.bindVertexBuffer(vertexBuffer);
					cmd.bindIndexBuffer(indexBuffer);
					cmd.drawIndexed(3);
					++recorded; });
				if (error)
					errors.push_back(error.message());
			}
		}
	};
} // namespace

TEST_CASE("Renderers on two threads share a Device through swapchain recreation", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	acmtest::requireInstance(instance);

	std::vector<acm::Surface> surfaces;
	for (int index = 0; index < 2; ++index)
	{
		surfaces.push_back(instance.createHeadlessSurface(kExtent));
		if (!surfaces.back().valid())
			SKIP("headless surface unavailable");
	}

	const acmtest::DeviceKind kind = acmtest::deviceKind(instance);
	acm::SurfaceOption option;
	bool found = false;
	for (const acm::SurfaceOption& candidate : instance.surfaceOptions(surfaces))
	{
		const acm::DeviceInfo& info = instance.devices()[candidate.device.deviceIndex];
		if (acmtest::deviceKindOf(info.type) == kind)
		{
			UNSCOPED_INFO("device: " << info.name << " (" << acmtest::deviceKindName(kind) << ")");
			option = candidate;
			found = true;
			break;
		}
	}
	if (!found)
		SKIP("the " << acmtest::deviceKindName(kind) << " device cannot present to two headless surfaces");

	acm::Device device = instance.createDevice(option.device);
	REQUIRE(device.valid());

	acm::PipelineShaders shaders;
	shaders.vertex = device.createShader(acmtest::vertexColorVertSpirv());
	shaders.fragment = device.createShader(acmtest::vertexColorFragSpirv());
	acm::PipelineConfig config;
	config.vertexLayout.stride = sizeof(Vertex);
	config.vertexLayout.attributes = {
		{0, acm::Format::R32G32_Sfloat, offsetof(Vertex, pos)},
		{1, acm::Format::R32G32B32_Sfloat, offsetof(Vertex, color)},
	};
	// Declared after the device, so every device resource is released before it.
	View views[2];
	for (size_t index = 0; index < 2; ++index)
	{
		View& view = views[index];
		view.swapChain = device.createSwapChain(surfaces[index], option, acm::SwapChainConfig{kExtent});
		if (!view.swapChain.valid())
			SKIP("driver does not support a headless swapchain");
		view.pipeline = device.createPipeline(shaders, view.swapChain.renderTarget(0), config);
		REQUIRE(view.pipeline.valid());
		view.renderer = device.createRenderer(view.swapChain);
		REQUIRE(view.renderer.valid());
	}

	std::vector<std::thread> threads;
	for (View& view : views)
		threads.emplace_back([&view, &device]
							 { view.run(device); });
	for (std::thread& thread : threads)
		thread.join();
	device.waitIdle();

	for (const View& view : views)
	{
		for (const std::string& error : view.errors)
			UNSCOPED_INFO("render thread error: " << error);
		CHECK(view.errors.empty());
		CHECK(view.recorded == kFrames);
		CHECK(view.recreated == kFrames / kRecreateEvery);
	}
}
