#include "test_spirv.h"
#include "vk_test_helpers.h"

#include <archimedes/archimedes.h>

#include <catch2/catch_all.hpp>
#include <cstdint>
#include <vector>

// Integration: MSAA. The same red triangle on black is rendered into two offscreen
// textures — single-sampled and 4x multisampled (resolved). Without MSAA every pixel
// is fully covered or not, so the red channel is exactly 0 or 255. With MSAA, the
// slanted edges get partial coverage that resolves to intermediate values. So the
// existence of intermediate-red pixels (and their absence in the 1x image) proves the
// multisample + resolve path worked. SKIPs if the device can't do 4x. Surface-free.

namespace
{
	int countPartialRed(const uint8_t* pixels, uint32_t w, uint32_t h)
	{
		int n = 0;
		for (uint32_t i = 0; i < w * h; ++i)
		{
			const uint8_t r = pixels[i * 4 + 2]; // [B,G,R,A]
			if (r > 40 && r < 215)
				++n;
		}
		return n;
	}
} // namespace

TEST_CASE("MSAA resolves edges to intermediate coverage", "[acm][gpu]")
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
	if (static_cast<int>(device.maxSampleCount()) < static_cast<int>(acm::SampleCount::Four))
		SKIP("device does not support 4x MSAA");

	constexpr uint32_t kSize = 64;
	const acm::Extent2D extent{kSize, kSize};

	auto renderTriangle = [&](acm::SampleCount samples, acm::Buffer& readback)
	{
		acm::Texture tex = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
		acm::RenderTarget target = device.createRenderTarget(tex, acm::RenderTargetFinish::CopySrc, /*depth*/ false, samples);
		REQUIRE(target.valid());

		acm::PipelineConfig config;
		config.vertex = device.createShader(acmtest::triangleVertSpirv());
		config.fragment = device.createShader(acmtest::triangleFragSpirv());
		config.target = target;
		config.samples = samples;
		acm::Pipeline pipeline = device.createPipeline(config);
		REQUIRE(pipeline.valid());

		readback = device.createBuffer(size_t(kSize) * kSize * 4, acm::BufferUsage::TransferDst);

		acm::CommandPool pool = device.createCommandPool();
		acm::CommandBuffer cmd = pool.allocate();
		cmd.begin();
		cmd.beginRenderPass(target);
		cmd.setViewportAndScissor(extent);
		cmd.bindPipeline(pipeline);
		cmd.draw(3);
		cmd.endRenderPass();
		cmd.copyTextureToBuffer(tex, readback);
		cmd.end();

		REQUIRE_FALSE(device.submitSync(cmd));
	};

	acm::Buffer single, multi;
	renderTriangle(acm::SampleCount::One, single);
	renderTriangle(acm::SampleCount::Four, multi);

	const auto* singlePx = static_cast<const uint8_t*>(single.map());
	const int singlePartial = countPartialRed(singlePx, kSize, kSize);
	single.unmap();

	const auto* multiPx = static_cast<const uint8_t*>(multi.map());
	const int multiPartial = countPartialRed(multiPx, kSize, kSize);
	multi.unmap();

	// 1x has hard edges (no partial-coverage pixels); 4x resolves them to intermediates.
	REQUIRE(singlePartial == 0);
	REQUIRE(multiPartial > 0);
}

// Per-sample shading has no readback-visible effect on a solid-colored triangle (it
// smooths aliasing *inside* high-frequency shaders), so this verifies the plumbing: a
// 4x MSAA pipeline with minSampleShading = 1 builds and renders correctly. SKIPs if the
// device can't do 4x or lacks sampleRateShading.
TEST_CASE("per-sample shading pipeline renders", "[acm][gpu]")
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
	if (static_cast<int>(device.maxSampleCount()) < static_cast<int>(acm::SampleCount::Four))
		SKIP("device does not support 4x MSAA");
	if (!device.enabledFeatures().sampleRateShading)
		SKIP("device does not support sampleRateShading");

	constexpr uint32_t kSize = 64;
	const acm::Extent2D extent{kSize, kSize};

	acm::Texture tex = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::RenderTarget target = device.createRenderTarget(tex, acm::RenderTargetFinish::CopySrc, /*depth*/ false, acm::SampleCount::Four);
	REQUIRE(target.valid());

	acm::PipelineConfig config;
	config.vertex = device.createShader(acmtest::triangleVertSpirv());
	config.fragment = device.createShader(acmtest::triangleFragSpirv());
	config.target = target;
	config.samples = acm::SampleCount::Four;
	config.minSampleShading = 1.0f; // shade every sample
	acm::Pipeline pipeline = device.createPipeline(config);
	REQUIRE(pipeline.valid());

	acm::Buffer readback = device.createBuffer(size_t(kSize) * kSize * 4, acm::BufferUsage::TransferDst);
	acm::CommandPool pool = device.createCommandPool();
	acm::CommandBuffer cmd = pool.allocate();
	cmd.begin();
	cmd.beginRenderPass(target);
	cmd.setViewportAndScissor(extent);
	cmd.bindPipeline(pipeline);
	cmd.draw(3);
	cmd.endRenderPass();
	cmd.copyTextureToBuffer(tex, readback);
	cmd.end();

	REQUIRE_FALSE(device.submitSync(cmd));

	// Center is inside the triangle: still the shader's red, sample shading or not.
	const auto* pixels = static_cast<const uint8_t*>(readback.map());
	const size_t center = (size_t(kSize / 2) * kSize + kSize / 2) * 4;
	const uint8_t b = pixels[center + 0], g = pixels[center + 1], r = pixels[center + 2];
	readback.unmap();
	REQUIRE(r > 200);
	REQUIRE(g < 60);
	REQUIRE(b < 60);
}
