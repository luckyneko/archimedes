#include "test_spirv.h"
#include "vk_test_helpers.h"
#include <archimedes/archimedes.h>
#include <catch2/catch_all.hpp>

// Integration: builds an acm::Shader + acm::Pipeline against a headless
// swapchain's render pass, using the precompiled SPIR-V in test_spirv.h. SKIPs
// without a live driver / headless surface / swapchain support.

TEST_CASE("Shader rejects empty SPIR-V, accepts valid", "[acm][gpu]")
{
	acmtest::HeadlessStack s;
	if (!acmtest::buildHeadlessStack(s))
		return;

	REQUIRE_FALSE(s.device.createShader(std::vector<char>{}).valid());

	acm::Shader vert = s.device.createShader(acmtest::triangleVertSpirv());
	REQUIRE(vert.valid());
	REQUIRE(vert.vkShaderModule() != VK_NULL_HANDLE);
}

TEST_CASE("Pipeline builds from shaders + render pass", "[acm][gpu]")
{
	acmtest::HeadlessStack s;
	if (!acmtest::buildHeadlessStack(s))
		return;

	acm::Shader vert = s.device.createShader(acmtest::triangleVertSpirv());
	acm::Shader frag = s.device.createShader(acmtest::triangleFragSpirv());
	REQUIRE(vert.valid());
	REQUIRE(frag.valid());

	acm::Pipeline pipeline = s.device.createPipeline(vert, frag, s.swapChain.vkRenderPass());
	REQUIRE(pipeline.valid());
	REQUIRE(pipeline.vkPipeline() != VK_NULL_HANDLE);
	REQUIRE(pipeline.vkPipelineLayout() != VK_NULL_HANDLE);

	// The shaders are only needed at creation time; dropping them must not affect
	// the already-built pipeline.
	vert.reset();
	frag.reset();
	REQUIRE(pipeline.valid());
}

TEST_CASE("Pipeline is invalid when a shader is", "[acm][gpu]")
{
	acmtest::HeadlessStack s;
	if (!acmtest::buildHeadlessStack(s))
		return;

	acm::Shader frag = s.device.createShader(acmtest::triangleFragSpirv());
	REQUIRE(frag.valid());

	// A null vertex shader can't build a pipeline.
	acm::Pipeline pipeline = s.device.createPipeline(acm::Shader(), frag, s.swapChain.vkRenderPass());
	REQUIRE_FALSE(pipeline.valid());
}
