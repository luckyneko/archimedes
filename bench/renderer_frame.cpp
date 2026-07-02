#include "bench_helpers.h"
#include "test_spirv.h"

#include <catch2/catch_all.hpp>

TEST_CASE("renderer_frame", "[bench][fast][gpu]")
{
	acmtest::HeadlessStack stack;
	if (!acmtest::buildHeadlessStack(stack))
		return;

	acm::Shader vertex = stack.device.createShader(acmtest::triangleVertSpirv());
	acm::Shader fragment = stack.device.createShader(acmtest::triangleFragSpirv());
	acm::Pipeline pipeline = stack.device.createPipeline(vertex, fragment, stack.swapChain.getRenderTarget(0));
	acm::Renderer renderer = stack.device.createRenderer(stack.swapChain);
	REQUIRE(pipeline.valid());
	REQUIRE(renderer.valid());

	BENCHMARK("acm(empty-frame)")
	{
		acm::Error error = renderer.render([](acm::CommandBuffer&, uint32_t) {});
		return !error;
	};

	BENCHMARK("acm(simple-draw-frame)")
	{
		acm::Error error = renderer.render([&](acm::CommandBuffer& commandBuffer, uint32_t)
										   {
			commandBuffer.bindPipeline(pipeline);
			commandBuffer.draw(3); });
		return !error;
	};

	stack.device.waitIdle();
}
