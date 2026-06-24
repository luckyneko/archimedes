#include "test_spirv.h"
#include "vk_test_helpers.h"

#include <archimedes/archimedes.h>

#include <catch2/catch_all.hpp>
#include <utility>

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
	acm::Shader retained = vert;
	vert.reset();
	REQUIRE_FALSE(vert.valid());
	REQUIRE(retained.valid());
}

TEST_CASE("Pipeline builds from shaders + render target", "[acm][gpu]")
{
	acmtest::HeadlessStack s;
	if (!acmtest::buildHeadlessStack(s))
		return;

	acm::Shader vert = s.device.createShader(acmtest::triangleVertSpirv());
	acm::Shader frag = s.device.createShader(acmtest::triangleFragSpirv());
	REQUIRE(vert.valid());
	REQUIRE(frag.valid());

	acm::Pipeline pipeline = s.device.createPipeline(vert, frag, s.swapChain.getRenderTarget(0));
	REQUIRE(pipeline.valid());

	acm::Pipeline retained = pipeline;
	pipeline.reset();
	REQUIRE_FALSE(pipeline.valid());
	REQUIRE(retained.valid());
	pipeline = std::move(retained);
	REQUIRE(pipeline.valid());
	REQUIRE_FALSE(retained.valid());

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
	acm::Pipeline pipeline = s.device.createPipeline(acm::Shader(), frag, s.swapChain.getRenderTarget(0));
	REQUIRE_FALSE(pipeline.valid());
}
