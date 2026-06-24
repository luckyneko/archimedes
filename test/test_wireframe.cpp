#include "test_spirv.h"
#include "vk_test_helpers.h"

#include <archimedes/archimedes.h>

#include <catch2/catch_all.hpp>
#include <cstdint>

// Integration: wireframe via the device's fillModeNonSolid feature. The same triangle
// is rasterized Fill (the interior is filled — center red) and Line (only edges are
// drawn — the center, well inside the triangle, stays the clear color). A black center
// under Line proves PolygonMode::Line took effect. SKIPs if the device couldn't enable
// the feature. Surface-free.

TEST_CASE("wireframe polygon mode draws only edges", "[acm][gpu]")
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
	if (!device.enabledFeatures().fillModeNonSolid)
		SKIP("device does not support fillModeNonSolid (wireframe)");

	constexpr uint32_t kSize = 64;
	const acm::Extent2D extent{kSize, kSize};

	acm::Texture texFill = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::Texture texWire = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::RenderTarget targetFill = device.createRenderTarget(texFill, acm::RenderTargetFinish::CopySrc);
	acm::RenderTarget targetWire = device.createRenderTarget(texWire, acm::RenderTargetFinish::CopySrc);
	REQUIRE(targetFill.valid());
	REQUIRE(targetWire.valid());

	auto makePipeline = [&](acm::RenderTarget target, acm::PolygonMode mode)
	{
		acm::PipelineConfig config;
		config.vertex = device.createShader(acmtest::triangleVertSpirv());
		config.fragment = device.createShader(acmtest::triangleFragSpirv());
		config.target = target;
		config.polygonMode = mode;
		return device.createPipeline(config);
	};
	acm::Pipeline pipeFill = makePipeline(targetFill, acm::PolygonMode::Fill);
	acm::Pipeline pipeWire = makePipeline(targetWire, acm::PolygonMode::Line);
	REQUIRE(pipeFill.valid());
	REQUIRE(pipeWire.valid());

	acm::Buffer rbFill = device.createBuffer(size_t(kSize) * kSize * 4, acm::BufferUsage::TransferDst);
	acm::Buffer rbWire = device.createBuffer(size_t(kSize) * kSize * 4, acm::BufferUsage::TransferDst);

	acm::CommandPool pool = device.createCommandPool();
	acm::CommandBuffer cmd = pool.allocate();
	REQUIRE(cmd.valid());

	cmd.begin();
	cmd.beginRenderPass(targetFill);
	cmd.setViewportAndScissor(extent);
	cmd.bindPipeline(pipeFill);
	cmd.draw(3);
	cmd.endRenderPass();
	cmd.copyTextureToBuffer(texFill, rbFill);

	cmd.beginRenderPass(targetWire);
	cmd.setViewportAndScissor(extent);
	cmd.bindPipeline(pipeWire);
	cmd.draw(3);
	cmd.endRenderPass();
	cmd.copyTextureToBuffer(texWire, rbWire);
	cmd.end();

	REQUIRE_FALSE(device.submitSync(cmd));

	const size_t center = (size_t(kSize / 2) * kSize + kSize / 2) * 4;
	const uint8_t* fill = static_cast<const uint8_t*>(rbFill.map()) + center;
	const uint8_t* wire = static_cast<const uint8_t*>(rbWire.map()) + center;

	// Fill: interior is red. Wireframe: interior (no edge here) stays black. [B,G,R,A].
	REQUIRE((fill[2] > 200 && fill[1] < 60 && fill[0] < 60));
	REQUIRE((wire[2] < 60 && wire[1] < 60 && wire[0] < 60));

	rbFill.unmap();
	rbWire.unmap();
}
