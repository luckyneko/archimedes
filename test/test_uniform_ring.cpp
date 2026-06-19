#include "test_spirv.h"
#include "vk_test_helpers.h"
#include <archimedes/archimedes.h>
#include <catch2/catch_all.hpp>
#include <cstdint>

// Integration: the UniformRing helper — a per-frame ring of uniform buffers +
// one-binding descriptor sets. We make a 2-deep ring of a fragment-stage color,
// write slot 0 green and slot 1 red, then render each slot into its own texture and
// read it back. Each result must match the value written to *its* slot, proving the
// ring keeps independent per-slot buffers/sets (no clobbering). Surface-free.

TEST_CASE("UniformRing keeps independent per-frame buffers", "[acm][gpu]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0, 0});
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

	// Two textures, one per ring slot, each left ready to copy out.
	acm::Texture tex0 = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::Texture tex1 = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::RenderTarget target0 = device.createRenderTarget(tex0, acm::RenderTargetFinish::CopySrc);
	acm::RenderTarget target1 = device.createRenderTarget(tex1, acm::RenderTargetFinish::CopySrc);
	REQUIRE(target0.valid());
	REQUIRE(target1.valid());

	// A 2-deep ring of a fragment-stage vec4 at binding 0.
	acm::UniformRing ring = device.createUniformRing(sizeof(float) * 4, 0, acm::ShaderStage::Fragment, 2);
	REQUIRE(ring.valid());
	REQUIRE(ring.frames() == 2);

	const float green[4] = {0.0f, 1.0f, 0.0f, 1.0f};
	const float red[4] = {1.0f, 0.0f, 0.0f, 1.0f};
	ring.update(0, green, sizeof(green));
	ring.update(1, red, sizeof(red));

	acm::PipelineConfig config;
	config.vertex = device.createShader(acmtest::triangleVertSpirv()); // hardcoded triangle
	config.fragment = device.createShader(acmtest::colorUniformFragSpirv());
	config.renderPass = target0.vkRenderPass();
	config.descriptorLayout = ring.descriptorLayout();
	acm::Pipeline pipeline = device.createPipeline(config);
	REQUIRE(pipeline.valid());

	acm::Buffer readback0 = device.createBuffer(size_t(kSize) * kSize * 4, acm::BufferUsage::TransferDst);
	acm::Buffer readback1 = device.createBuffer(size_t(kSize) * kSize * 4, acm::BufferUsage::TransferDst);
	REQUIRE(readback0.valid());
	REQUIRE(readback1.valid());

	acm::CommandPool pool = device.createCommandPool();
	acm::CommandBuffer cmd = pool.allocate();
	REQUIRE(cmd.valid());

	cmd.begin();

	cmd.beginRenderPass(target0);
	cmd.setViewportAndScissor(extent);
	cmd.bindPipeline(pipeline);
	cmd.bindDescriptorSet(pipeline, ring.descriptorSet(0));
	cmd.draw(3);
	cmd.endRenderPass();
	cmd.copyTextureToBuffer(tex0, readback0);

	cmd.beginRenderPass(target1);
	cmd.setViewportAndScissor(extent);
	cmd.bindPipeline(pipeline);
	cmd.bindDescriptorSet(pipeline, ring.descriptorSet(1));
	cmd.draw(3);
	cmd.endRenderPass();
	cmd.copyTextureToBuffer(tex1, readback1);

	cmd.end();

	VkCommandBuffer vkcb = cmd.vkCommandBuffer();
	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &vkcb;
	REQUIRE(vkQueueSubmit(device.vkQueue(), 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS);
	REQUIRE(vkQueueWaitIdle(device.vkQueue()) == VK_SUCCESS);

	// Center pixel of each must be that slot's color. B8G8R8A8 layout is [B,G,R,A].
	const size_t center = (size_t(kSize / 2) * kSize + kSize / 2) * 4;

	const auto* p0 = static_cast<const uint8_t*>(readback0.map());
	REQUIRE(p0 != nullptr);
	const uint8_t b0 = p0[center + 0], g0 = p0[center + 1], r0 = p0[center + 2];
	readback0.unmap();

	const auto* p1 = static_cast<const uint8_t*>(readback1.map());
	REQUIRE(p1 != nullptr);
	const uint8_t b1 = p1[center + 0], g1 = p1[center + 1], r1 = p1[center + 2];
	readback1.unmap();

	// Slot 0 stayed green, slot 1 stayed red — neither write clobbered the other.
	REQUIRE(g0 > 200);
	REQUIRE(r0 < 60);
	REQUIRE(b0 < 60);

	REQUIRE(r1 > 200);
	REQUIRE(g1 < 60);
	REQUIRE(b1 < 60);
}
