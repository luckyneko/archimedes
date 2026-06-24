#include "test_spirv.h"
#include "vk_test_helpers.h"

#include <archimedes/archimedes.h>

#include <catch2/catch_all.hpp>
#include <cstdint>
#include <vector>

// Integration: anisotropic sampling. The visible payoff (sharper minified textures at
// oblique angles) needs mipmaps + an angled view to show up in a flat readback, which
// this renderer doesn't have yet — so this verifies the *plumbing*: the samplerAnisotropy
// feature is enabled, an anisotropic Sampler builds, and it still samples correctly
// (an uploaded green texture comes back green through a 16x sampler). SKIPs if the
// device couldn't enable the feature. Surface-free.

TEST_CASE("an anisotropic sampler samples correctly", "[acm][gpu]")
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
	if (!device.enabledFeatures().samplerAnisotropy)
		SKIP("device does not support samplerAnisotropy");

	constexpr uint32_t kSize = 64;
	const acm::Extent2D extent{kSize, kSize};

	std::vector<uint8_t> pixels(size_t(kSize) * kSize * 4);
	for (size_t i = 0; i < pixels.size(); i += 4)
	{
		pixels[i + 0] = 0;	 // B
		pixels[i + 1] = 255; // G
		pixels[i + 2] = 0;	 // R
		pixels[i + 3] = 255; // A
	}

	acm::Texture texA = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	REQUIRE(texA.valid());
	texA.upload(pixels.data(), pixels.size());

	acm::Texture texB = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::RenderTarget targetB = device.createRenderTarget(texB, acm::RenderTargetFinish::CopySrc);
	REQUIRE(targetB.valid());

	// The point of the test: a 16x anisotropic sampler (clamped to the device limit).
	acm::Sampler sampler = device.createSampler(16.0f);
	REQUIRE(sampler.valid());
	acm::DescriptorSetLayout layout = device.createDescriptorSetLayout(1);
	acm::DescriptorSet descriptors = device.createDescriptorSet(layout);
	REQUIRE(descriptors.valid());
	descriptors.setTexture(0, texA, sampler);

	acm::PipelineConfig config;
	config.vertex = device.createShader(acmtest::fullscreenVertSpirv());
	config.fragment = device.createShader(acmtest::sampleTextureFragSpirv());
	config.target = targetB;
	config.descriptorLayout = layout;
	acm::Pipeline pipeline = device.createPipeline(config);
	REQUIRE(pipeline.valid());

	acm::Buffer readback = device.createBuffer(size_t(kSize) * kSize * 4, acm::BufferUsage::TransferDst);
	REQUIRE(readback.valid());

	acm::CommandPool pool = device.createCommandPool();
	acm::CommandBuffer cmd = pool.allocate();
	REQUIRE(cmd.valid());

	cmd.begin();
	cmd.beginRenderPass(targetB);
	cmd.setViewportAndScissor(extent);
	cmd.bindPipeline(pipeline);
	cmd.bindDescriptorSet(pipeline, descriptors);
	cmd.draw(3);
	cmd.endRenderPass();
	cmd.copyTextureToBuffer(texB, readback);
	cmd.end();

	REQUIRE_FALSE(device.submitSync(cmd));

	const auto* out = static_cast<const uint8_t*>(readback.map());
	REQUIRE(out != nullptr);
	const size_t center = (size_t(kSize / 2) * kSize + kSize / 2) * 4;
	const uint8_t b = out[center + 0];
	const uint8_t g = out[center + 1];
	const uint8_t r = out[center + 2];
	readback.unmap();

	REQUIRE(g > 200);
	REQUIRE(r < 60);
	REQUIRE(b < 60);
}
