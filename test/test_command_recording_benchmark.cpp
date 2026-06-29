#include "test_spirv.h"
#include "vk_test_helpers.h"

#include <archimedes/archimedes.h>

#include <catch2/catch_all.hpp>

TEST_CASE("production command-recording benchmark", "[acm][gpu][benchmark]")
{
	acm::Instance instance("acm-command-benchmark", acm::Version{0, 1, 0});
	if (!instance.valid())
		SKIP("no Vulkan driver available");

	uint32_t queueIndex = 0;
	const acm::GPU* gpu = acmtest::selectGraphicsGPU(instance, queueIndex);
	if (!gpu)
		SKIP("no graphics-capable queue family");

	acm::Device device = instance.createDevice(*gpu, queueIndex);
	REQUIRE(device.valid());

	const acm::Extent2D extent{64, 64};
	acm::Texture texture = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::RenderTarget target = device.createRenderTarget(texture, acm::RenderTargetFinish::CopySrc);
	acm::Shader vertex = device.createShader(acmtest::triangleVertSpirv());
	acm::Shader fragment = device.createShader(acmtest::triangleFragSpirv());
	acm::Pipeline pipeline = device.createPipeline(vertex, fragment, target);
	acm::Buffer vertexBuffer = device.createBuffer(256, acm::BufferUsage::Vertex);
	acm::CommandPool pool = device.createCommandPool();
	acm::CommandBuffer commandBuffer = pool.allocate();
	REQUIRE(target.valid());
	REQUIRE(pipeline.valid());
	REQUIRE(vertexBuffer.valid());
	REQUIRE(commandBuffer.valid());

	BENCHMARK("record 10000 production draw sequences")
	{
		REQUIRE_FALSE(commandBuffer.begin());
		commandBuffer.beginRendering(target);
		commandBuffer.setViewportAndScissor(extent);
		for (uint32_t draw = 0; draw < 10000; ++draw)
		{
			commandBuffer.bindPipeline(pipeline);
			commandBuffer.bindVertexBuffer(vertexBuffer);
			commandBuffer.draw(3);
		}
		commandBuffer.endRendering();
		REQUIRE_FALSE(commandBuffer.end());
		return commandBuffer.valid();
	};
}
