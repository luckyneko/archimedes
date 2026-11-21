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
#include <cstdint>
#include <string>
#include <vector>

// Integration: every CommandBuffer recording call that rejects its input records a sticky
// error. The first rejected call since begin() is the one error() reports; each call still
// returns its own error. begin() clears it, so a later good recording is clean and renders.
// Surface-free except the stale-target case, which needs a headless swapchain.

namespace
{
	constexpr uint32_t kSize = 32;

	// The resources a recording needs, all on one device, so each check can pass one bad
	// argument next to otherwise good ones.
	struct Scene
	{
		acm::Texture color;
		acm::RenderTarget target;
		acm::Pipeline pipeline;		  // no descriptor layout
		acm::Pipeline layoutPipeline; // descriptor layout: one fragment uniform at binding 0
		acm::DescriptorSetLayout layout;
		acm::DescriptorSet set;
		acm::Buffer uniform;
		acm::Buffer readback;
		acm::Buffer storage;
		acm::DescriptorSetLayout computeLayout;
		acm::DescriptorSet computeSet;
		acm::ComputePipeline compute;
		acm::CommandPool pool;
	};

	Scene buildScene(acm::Device& device)
	{
		const acm::Extent2D extent{kSize, kSize};
		Scene s;
		s.color = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
		s.target = device.createRenderTarget(s.color, acm::RenderTargetConfig{acm::RenderTargetFinish::CopySrc});
		REQUIRE(s.target.valid());
		const acm::PipelineShaders triangle{device.createShader(acmtest::triangleVertSpirv()), device.createShader(acmtest::triangleFragSpirv())};
		s.pipeline = device.createPipeline(triangle, s.target);
		REQUIRE(s.pipeline.valid());

		s.layout = device.createDescriptorSetLayout({{0, acm::DescriptorType::UniformBuffer, acm::ShaderStage::Fragment}});
		REQUIRE(s.layout.valid());
		acm::PipelineConfig config;
		config.descriptorLayout = s.layout;
		s.layoutPipeline = device.createPipeline(acm::PipelineShaders{device.createShader(acmtest::triangleVertSpirv()), device.createShader(acmtest::colorUniformFragSpirv())}, s.target, config);
		REQUIRE(s.layoutPipeline.valid());
		const float red[4] = {1.0f, 0.0f, 0.0f, 1.0f};
		s.uniform = device.createBuffer(sizeof(red), acm::BufferUsage::Uniform);
		REQUIRE_FALSE(s.uniform.write(red, sizeof(red)));
		s.set = device.createDescriptorSet(s.layout);
		REQUIRE(s.set.valid());
		s.set.setBuffer(0, s.uniform);

		s.readback = device.createBuffer(size_t(kSize) * kSize * 4, acm::BufferUsage::TransferDst);
		REQUIRE(s.readback.valid());

		s.storage = device.createBuffer(256 * sizeof(uint32_t), acm::BufferUsage::Storage);
		REQUIRE(s.storage.valid());
		s.computeLayout = device.createDescriptorSetLayout({{0, acm::DescriptorType::StorageBuffer, acm::ShaderStage::Compute}});
		s.computeSet = device.createDescriptorSet(s.computeLayout);
		REQUIRE(s.computeSet.valid());
		s.computeSet.setBuffer(0, s.storage);
		s.compute = device.createComputePipeline(device.createShader(acmtest::computeFillSpirv()), s.computeLayout);
		REQUIRE(s.compute.valid());

		s.pool = device.createCommandPool();
		REQUIRE(s.pool.valid());
		return s;
	}

	// Renders the red triangle and reads the center back: proof a recording after a failed
	// one is clean and actually executes.
	void requireCleanRedFrame(acm::Device& device, Scene& s, acm::CommandBuffer& cmd)
	{
		REQUIRE_FALSE(cmd.begin());
		REQUIRE_FALSE(cmd.error());
		REQUIRE_FALSE(cmd.beginRendering(s.target));
		cmd.setViewportAndScissor({kSize, kSize});
		REQUIRE_FALSE(cmd.bindPipeline(s.pipeline));
		REQUIRE_FALSE(cmd.draw(3));
		cmd.endRendering();
		cmd.copyTextureToBuffer(s.color, s.readback);
		REQUIRE_FALSE(cmd.error());
		REQUIRE_FALSE(cmd.end());
		REQUIRE_FALSE(device.submitSync(cmd));

		const auto* px = static_cast<const uint8_t*>(s.readback.map());
		REQUIRE(px != nullptr);
		const size_t center = (size_t(kSize / 2) * kSize + kSize / 2) * 4;
		const uint8_t b = px[center + 0], g = px[center + 1], r = px[center + 2];
		s.readback.unmap();
		REQUIRE(r > 200);
		REQUIRE(g < 60);
		REQUIRE(b < 60);
	}

	// One rejected recording call: `record` runs after begin() (inside a rendering scope over
	// the scene's target when `inScope`), must leave a sticky error mentioning `expected`,
	// and the next begin() must start clean.
	template <typename Record>
	void requireRejected(Scene& s, acm::CommandBuffer& cmd, bool inScope, const std::string& expected, Record&& record)
	{
		REQUIRE_FALSE(cmd.begin());
		if (inScope)
		{
			REQUIRE_FALSE(cmd.beginRendering(s.target));
			cmd.setViewportAndScissor({kSize, kSize});
		}
		REQUIRE_FALSE(cmd.error());
		record(cmd);
		REQUIRE(cmd.error());
		CHECK_THAT(cmd.error().message(), Catch::Matchers::ContainsSubstring(expected));
		if (inScope)
			cmd.endRendering();
		cmd.end();
		REQUIRE_FALSE(cmd.begin());
		REQUIRE_FALSE(cmd.error());
		REQUIRE_FALSE(cmd.end());
	}
} // namespace

TEST_CASE("recording calls reject invalid arguments with a sticky error", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	acmtest::requireInstance(instance);
	uint32_t queueIndex = 0;
	const acm::DeviceInfo* gpu = acmtest::selectGraphicsDevice(instance, queueIndex);
	acm::Device device = instance.createDevice(*gpu, queueIndex);
	REQUIRE(device.valid());

	Scene s = buildScene(device);
	acm::CommandBuffer cmd = s.pool.allocate();
	REQUIRE(cmd.valid());

	SECTION("graphics binds")
	{
		requireRejected(s, cmd, true, "descriptor set", [&](acm::CommandBuffer& c)
						{ c.bindDescriptorSet(s.layoutPipeline, acm::DescriptorSet{}); });
		requireRejected(s, cmd, true, "pipeline", [&](acm::CommandBuffer& c)
						{ c.bindDescriptorSet(acm::Pipeline{}, s.set); });
		requireRejected(s, cmd, true, "descriptor set", [&](acm::CommandBuffer& c)
						{ c.bindDescriptorSet(s.layoutPipeline, acm::DescriptorSet{}, 0); });
		// The plain pipeline has no descriptor layout, so no set can be bound against it.
		requireRejected(s, cmd, true, "descriptor layout", [&](acm::CommandBuffer& c)
						{ c.bindDescriptorSet(s.pipeline, s.set); });
		// The set has no dynamic uniform, so a dynamic offset has nothing to apply to.
		requireRejected(s, cmd, true, "dynamic", [&](acm::CommandBuffer& c)
						{ c.bindDescriptorSet(s.layoutPipeline, s.set, 0); });
		requireRejected(s, cmd, true, "vertex buffer", [&](acm::CommandBuffer& c)
						{ c.bindVertexBuffer(acm::Buffer{}); });
		requireRejected(s, cmd, true, "index buffer", [&](acm::CommandBuffer& c)
						{ c.bindIndexBuffer(acm::Buffer{}); });
		requireRejected(s, cmd, true, "extent", [&](acm::CommandBuffer& c)
						{ c.setViewportAndScissor({0, 0}); });
	}

	SECTION("rendering scope")
	{
		requireRejected(s, cmd, false, "rendering scope", [&](acm::CommandBuffer& c)
						{ c.endRendering(); });
		// Transfers and barriers belong outside dynamic rendering.
		requireRejected(s, cmd, true, "rendering scope", [&](acm::CommandBuffer& c)
						{ c.bufferBarrier(s.storage, acm::ShaderStage::Compute, acm::ShaderStage::Fragment); });
		requireRejected(s, cmd, true, "rendering scope", [&](acm::CommandBuffer& c)
						{ c.dispatch(1); });
	}

	SECTION("transfer and synchronization")
	{
		requireRejected(s, cmd, false, "texture", [&](acm::CommandBuffer& c)
						{ c.copyTextureToBuffer(acm::Texture{}, s.readback); });
		requireRejected(s, cmd, false, "buffer", [&](acm::CommandBuffer& c)
						{ c.copyTextureToBuffer(s.color, acm::Buffer{}); });
		// 32x32 BGRA8 needs 4096 bytes; the uniform holds 16.
		requireRejected(s, cmd, false, "too small", [&](acm::CommandBuffer& c)
						{ c.copyTextureToBuffer(s.color, s.uniform); });
		requireRejected(s, cmd, false, "buffer", [&](acm::CommandBuffer& c)
						{ c.bufferBarrier(acm::Buffer{}, acm::ShaderStage::Compute, acm::ShaderStage::Vertex); });
		requireRejected(s, cmd, false, "stage", [&](acm::CommandBuffer& c)
						{ c.bufferBarrier(s.storage, acm::ShaderStage(0), acm::ShaderStage::Vertex); });
		requireRejected(s, cmd, false, "texture", [&](acm::CommandBuffer& c)
						{ c.transitionImage(acm::Texture{}, acm::ImageLayout::Undefined, acm::ImageLayout::General); });
		requireRejected(s, cmd, false, "Undefined", [&](acm::CommandBuffer& c)
						{ c.transitionImage(s.color, acm::ImageLayout::General, acm::ImageLayout::Undefined); });
	}

	SECTION("compute")
	{
		requireRejected(s, cmd, false, "compute pipeline", [&](acm::CommandBuffer& c)
						{ c.bindComputePipeline(acm::ComputePipeline{}); });
		requireRejected(s, cmd, false, "compute pipeline", [&](acm::CommandBuffer& c)
						{ c.bindComputeDescriptorSet(acm::ComputePipeline{}, s.computeSet); });
		requireRejected(s, cmd, false, "descriptor set", [&](acm::CommandBuffer& c)
						{ c.bindComputeDescriptorSet(s.compute, acm::DescriptorSet{}); });
		requireRejected(s, cmd, false, "compute pipeline", [&](acm::CommandBuffer& c)
						{ c.dispatch(1); });
	}

	SECTION("recording outside begin/end")
	{
		REQUIRE_FALSE(cmd.begin());
		REQUIRE_FALSE(cmd.end());
		cmd.bindVertexBuffer(s.uniform); // after end(): the buffer is no longer recording
		REQUIRE(cmd.error());
		CHECK_THAT(cmd.error().message(), Catch::Matchers::ContainsSubstring("not recording"));
	}

	requireCleanRedFrame(device, s, cmd);
}

TEST_CASE("the first recording error wins and copies share it", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	acmtest::requireInstance(instance);
	uint32_t queueIndex = 0;
	const acm::DeviceInfo* gpu = acmtest::selectGraphicsDevice(instance, queueIndex);
	acm::Device device = instance.createDevice(*gpu, queueIndex);
	REQUIRE(device.valid());

	Scene s = buildScene(device);
	acm::CommandBuffer cmd = s.pool.allocate();
	REQUIRE(cmd.valid());

	REQUIRE_FALSE(cmd.begin());
	REQUIRE_FALSE(cmd.beginRendering(s.target));
	cmd.bindVertexBuffer(acm::Buffer{});
	const acm::Error first = cmd.error();
	REQUIRE(first);

	// A later failure returns its own error but does not replace the first.
	const acm::Error later = cmd.draw(3); // no pipeline bound
	REQUIRE(later);
	REQUIRE(later.message() != first.message());
	REQUIRE(cmd.error().message() == first.message());

	// The error lives with the command buffer, not the wrapper: a copy sees it, and a
	// failure recorded through the copy is visible through the original.
	acm::CommandBuffer copy = cmd;
	REQUIRE(copy.error().message() == first.message());
	cmd.endRendering();
	REQUIRE_FALSE(cmd.end());
	REQUIRE_FALSE(copy.begin());
	REQUIRE_FALSE(cmd.error());
	copy.bindIndexBuffer(acm::Buffer{});
	REQUIRE(cmd.error());
	REQUIRE_FALSE(copy.end());

	// An invalid wrapper reports its own error from every call.
	acm::CommandBuffer invalid;
	invalid.bindVertexBuffer(s.uniform);
	REQUIRE(invalid.error());
	REQUIRE(invalid.begin());

	requireCleanRedFrame(device, s, cmd);
}

TEST_CASE("recording rejects resources owned by another device", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	acmtest::requireInstance(instance);
	uint32_t queueIndex = 0;
	const acm::DeviceInfo* gpu = acmtest::selectGraphicsDevice(instance, queueIndex);
	acm::Device device = instance.createDevice(*gpu, queueIndex);
	acm::Device other = instance.createDevice(*gpu, queueIndex);
	REQUIRE(device.valid());
	REQUIRE(other.valid());

	Scene s = buildScene(device);
	Scene o = buildScene(other);
	acm::CommandBuffer cmd = s.pool.allocate();
	REQUIRE(cmd.valid());

	requireRejected(s, cmd, true, "another device", [&](acm::CommandBuffer& c)
					{ c.bindDescriptorSet(s.layoutPipeline, o.set); });
	requireRejected(s, cmd, true, "another device", [&](acm::CommandBuffer& c)
					{ c.bindDescriptorSet(o.layoutPipeline, s.set); });
	requireRejected(s, cmd, true, "another device", [&](acm::CommandBuffer& c)
					{ c.bindVertexBuffer(o.uniform); });
	requireRejected(s, cmd, true, "another device", [&](acm::CommandBuffer& c)
					{ c.bindIndexBuffer(o.uniform); });
	requireRejected(s, cmd, false, "another device", [&](acm::CommandBuffer& c)
					{ c.copyTextureToBuffer(o.color, s.readback); });
	requireRejected(s, cmd, false, "another device", [&](acm::CommandBuffer& c)
					{ c.bufferBarrier(o.storage, acm::ShaderStage::Compute, acm::ShaderStage::Vertex); });
	requireRejected(s, cmd, false, "another device", [&](acm::CommandBuffer& c)
					{ c.transitionImage(o.color, acm::ImageLayout::Undefined, acm::ImageLayout::General); });
	requireRejected(s, cmd, false, "another device", [&](acm::CommandBuffer& c)
					{ c.bindComputePipeline(o.compute); });
	requireRejected(s, cmd, false, "another device", [&](acm::CommandBuffer& c)
					{ c.bindComputeDescriptorSet(s.compute, o.computeSet); });

	requireCleanRedFrame(device, s, cmd);
}

TEST_CASE("submitSync reports a failed recording and does not submit it", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	acmtest::requireInstance(instance);
	uint32_t queueIndex = 0;
	const acm::DeviceInfo* gpu = acmtest::selectGraphicsDevice(instance, queueIndex);
	acm::Device device = instance.createDevice(*gpu, queueIndex);
	REQUIRE(device.valid());

	Scene s = buildScene(device);
	const std::vector<uint32_t> zeros(256, 0);
	REQUIRE_FALSE(s.storage.write(zeros.data(), zeros.size() * sizeof(uint32_t)));

	// A valid dispatch followed by a rejected barrier: the whole recording is refused.
	const acm::Error error = device.submitSync([&](acm::CommandBuffer& cmd)
											   {
												   cmd.bindComputePipeline(s.compute);
												   cmd.bindComputeDescriptorSet(s.compute, s.computeSet);
												   cmd.dispatch(4);
												   cmd.bufferBarrier(acm::Buffer{}, acm::ShaderStage::Compute, acm::ShaderStage::Vertex); });
	REQUIRE(error);
	CHECK_THAT(error.message(), Catch::Matchers::ContainsSubstring("buffer"));
	const auto* values = static_cast<const uint32_t*>(s.storage.map());
	REQUIRE(values != nullptr);
	REQUIRE(values[1] == 0u); // the dispatch never ran
	s.storage.unmap();

	// The same work without the bad barrier runs.
	REQUIRE_FALSE(device.submitSync([&](acm::CommandBuffer& cmd)
									{
										cmd.bindComputePipeline(s.compute);
										cmd.bindComputeDescriptorSet(s.compute, s.computeSet);
										cmd.dispatch(4); }));
	values = static_cast<const uint32_t*>(s.storage.map());
	REQUIRE(values != nullptr);
	REQUIRE(values[1] == 3u);
	s.storage.unmap();
}

TEST_CASE("beginRendering rejects a target retired by swapchain recreation", "[acm][gpu]")
{
	acmtest::HeadlessStack stack;
	if (!acmtest::buildHeadlessStack(stack))
		return;

	acm::RenderTarget stale = stack.swapChain.renderTarget(0);
	REQUIRE(stale.valid());
	REQUIRE(stack.swapChain.recreate());
	REQUIRE_FALSE(stale.valid());

	acm::CommandPool pool = stack.device.createCommandPool();
	acm::CommandBuffer cmd = pool.allocate();
	REQUIRE_FALSE(cmd.begin());
	REQUIRE(cmd.beginRendering(stale));
	REQUIRE(cmd.error());
	REQUIRE_FALSE(cmd.end());

	// A fresh target from the recreated swapchain records cleanly after begin().
	REQUIRE_FALSE(cmd.begin());
	REQUIRE_FALSE(cmd.beginRendering(stack.swapChain.renderTarget(0)));
	cmd.endRendering();
	REQUIRE_FALSE(cmd.error());
	REQUIRE_FALSE(cmd.end());
}
