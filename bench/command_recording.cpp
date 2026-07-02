#include "bench_helpers.h"
#include "test_spirv.h"

#include <catch2/catch_all.hpp>

TEST_CASE("command_recording", "[bench][fast][gpu]")
{
	acmbench::DeviceStack stack;
	if (!acmbench::buildGraphicsDevice(stack, "acm-command-benchmark"))
		return;

	const acm::Extent2D extent{64, 64};
	acm::Texture texture = stack.device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::RenderTarget target = stack.device.createRenderTarget(texture, acm::RenderTargetFinish::CopySrc);
	acm::Shader vertex = stack.device.createShader(acmtest::triangleVertSpirv());
	acm::Shader fragment = stack.device.createShader(acmtest::triangleFragSpirv());
	acm::Pipeline pipeline = stack.device.createPipeline(vertex, fragment, target);
	acm::Buffer vertexBuffer = stack.device.createBuffer(256, acm::BufferUsage::Vertex);
	acm::CommandPool pool = stack.device.createCommandPool();
	acm::CommandBuffer commandBuffer = pool.allocate();
	REQUIRE(target.valid());
	REQUIRE(pipeline.valid());
	REQUIRE(vertexBuffer.valid());
	REQUIRE(commandBuffer.valid());

	auto recordEmpty = [&commandBuffer]
	{
		if (commandBuffer.begin())
			return false;
		return !commandBuffer.end() && commandBuffer.valid();
	};

	auto recordDraws = [&](uint32_t drawCount)
	{
		if (commandBuffer.begin())
			return false;
		commandBuffer.beginRendering(target);
		commandBuffer.setViewportAndScissor(extent);
		for (uint32_t draw = 0; draw < drawCount; ++draw)
		{
			commandBuffer.bindPipeline(pipeline);
			commandBuffer.bindVertexBuffer(vertexBuffer);
			commandBuffer.draw(3);
		}
		commandBuffer.endRendering();
		return !commandBuffer.end() && commandBuffer.valid();
	};

	BENCHMARK("acm(empty-begin-end)") { return recordEmpty(); };

	SECTION("100 draws")
	{
		BENCHMARK("acm(draw-sequence)") { return recordDraws(100); };
	}

	SECTION("1000 draws")
	{
		BENCHMARK("acm(draw-sequence)") { return recordDraws(1000); };
	}

	SECTION("10000 draws")
	{
		BENCHMARK("acm(draw-sequence)") { return recordDraws(10000); };
	}
}
