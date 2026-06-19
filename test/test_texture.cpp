#include "test_spirv.h"
#include "vk_test_helpers.h"
#include <archimedes/archimedes.h>
#include <catch2/catch_all.hpp>
#include <cstdint>

// Integration: the render-to-texture path. Renders the triangle into an owned
// acm::Texture (no swapchain / surface needed), copies it to a host-visible
// acm::Buffer, and reads back the center pixel — actual output verification, not
// just "the calls succeeded". Runs on any graphics-capable driver; SKIPs without.

TEST_CASE("render to texture produces a red triangle", "[acm][gpu]")
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

	acm::Texture texture = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	REQUIRE(texture.valid());
	REQUIRE(texture.vkImage() != VK_NULL_HANDLE);
	REQUIRE(texture.vkImageView() != VK_NULL_HANDLE);

	acm::RenderTarget target = device.createRenderTarget(texture, acm::RenderTargetFinish::CopySrc);
	REQUIRE(target.valid());

	acm::Shader vert = device.createShader(acmtest::triangleVertSpirv());
	acm::Shader frag = device.createShader(acmtest::triangleFragSpirv());
	acm::Pipeline pipeline = device.createPipeline(vert, frag, target.vkRenderPass());
	REQUIRE(pipeline.valid());

	acm::Buffer readback = device.createBuffer(size_t(kSize) * kSize * 4, acm::BufferUsage::TransferDst);
	REQUIRE(readback.valid());

	// Record: draw the triangle into the texture, then copy the texture out.
	acm::CommandPool pool = device.createCommandPool();
	acm::CommandBuffer cmd = pool.allocate();
	REQUIRE(cmd.valid());

	cmd.begin();
	cmd.beginRenderPass(target);
	cmd.setViewportAndScissor(extent);
	cmd.bindPipeline(pipeline);
	cmd.draw(3);
	cmd.endRenderPass();
	cmd.copyTextureToBuffer(texture, readback);
	cmd.end();

	VkCommandBuffer vkcb = cmd.vkCommandBuffer();
	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &vkcb;
	REQUIRE(vkQueueSubmit(device.vkQueue(), 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS);
	REQUIRE(vkQueueWaitIdle(device.vkQueue()) == VK_SUCCESS);

	// The center pixel sits inside the triangle, so it must be the shader's red.
	// B8G8R8A8 layout is [B, G, R, A].
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
