#include "test_spirv.h"
#include "vk_test_helpers.h"

#include <archimedes/archimedes.h>

#include <catch2/catch_all.hpp>
#include <cstdint>
#include <utility>

// Integration: the full render-to-texture-then-sample path. Pass 1 renders the
// red triangle into texture A (left in SHADER_READ_ONLY). Pass 2 draws a
// fullscreen triangle that samples A — via a Sampler + DescriptorSet — into
// texture B, which is then copied out and checked. Surface-free, so it runs on
// any graphics-capable driver.

TEST_CASE("sampling a rendered texture reproduces its color", "[acm][gpu]")
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

	constexpr uint32_t kSize = 64;
	const acm::Extent2D extent{kSize, kSize};

	// Pass 1: red triangle into A, left ready to sample.
	acm::Texture texA = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::RenderTarget targetA = device.createRenderTarget(texA, acm::RenderTargetFinish::Sampled);
	acm::Pipeline pipeA = device.createPipeline(device.createShader(acmtest::triangleVertSpirv()),
												device.createShader(acmtest::triangleFragSpirv()),
												targetA);
	REQUIRE(targetA.valid());
	REQUIRE(pipeA.valid());

	// Pass 2: fullscreen triangle samples A into B (left ready to copy out).
	acm::Texture texB = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::RenderTarget targetB = device.createRenderTarget(texB, acm::RenderTargetFinish::CopySrc);

	acm::Sampler sampler = device.createSampler();
	REQUIRE(sampler.valid());
	acm::Sampler retainedSampler = sampler;
	sampler.reset();
	REQUIRE_FALSE(sampler.valid());
	REQUIRE(retainedSampler.valid());
	sampler = std::move(retainedSampler);
	acm::DescriptorSetLayout layout = device.createDescriptorSetLayout(1);
	REQUIRE(layout.valid());
	acm::DescriptorSet descriptors = device.createDescriptorSet(layout);
	REQUIRE(descriptors.valid());
	descriptors.setTexture(0, texA, sampler);

	acm::PipelineConfig configB;
	configB.vertex = device.createShader(acmtest::fullscreenVertSpirv());
	configB.fragment = device.createShader(acmtest::sampleTextureFragSpirv());
	configB.target = targetB;
	configB.descriptorLayout = layout;
	acm::Pipeline pipeB = device.createPipeline(configB);
	REQUIRE(pipeB.valid());

	acm::Buffer readback = device.createBuffer(size_t(kSize) * kSize * 4, acm::BufferUsage::TransferDst);
	REQUIRE(readback.valid());

	acm::CommandPool pool = device.createCommandPool();
	acm::CommandBuffer cmd = pool.allocate();
	REQUIRE(cmd.valid());

	cmd.begin();

	cmd.beginRenderPass(targetA);
	cmd.setViewportAndScissor(extent);
	cmd.bindPipeline(pipeA);
	cmd.draw(3);
	cmd.endRenderPass();

	cmd.beginRenderPass(targetB);
	cmd.setViewportAndScissor(extent);
	cmd.bindPipeline(pipeB);
	cmd.bindDescriptorSet(pipeB, descriptors);
	cmd.draw(3);
	cmd.endRenderPass();

	cmd.copyTextureToBuffer(texB, readback);
	cmd.end();

	REQUIRE_FALSE(device.submitSync(cmd));

	// B's center sampled A's center (the red triangle). B8G8R8A8 layout is [B,G,R,A].
	const auto* pixels = static_cast<const uint8_t*>(readback.map());
	REQUIRE(pixels != nullptr);
	const size_t center = (size_t(kSize / 2) * kSize + kSize / 2) * 4;
	const uint8_t b = pixels[center + 0];
	const uint8_t g = pixels[center + 1];
	const uint8_t r = pixels[center + 2];
	readback.unmap();

	REQUIRE(r > 200);
	REQUIRE(g < 60);
	REQUIRE(b < 60);
}
