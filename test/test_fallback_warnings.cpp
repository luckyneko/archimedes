/*
 *  Created by LuckyNeko on 22/11/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "test_spirv.h"
#include "vk_test_helpers.h"

#include <archimedes/acmLog.h>
#include <archimedes/archimedes.h>

#include <catch2/catch_all.hpp>
#include <cstdint>
#include <initializer_list>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

// The degrade-with-a-warning fallbacks: a request the device cannot honour is replaced by
// what it can do, and the object that degraded logs one Warning (LogCategory::Library)
// naming what was requested and what is used instead. The warning comes at creation, once,
// never per draw or per swapchain rebuild, and an honoured request logs nothing. A device
// created with no optional feature (DeviceConfig::optionalFeatures empty) forces the
// feature-gated fallbacks on every device kind; the multisampling ones need a device that
// lacks 8x MSAA (the CPU device) and SKIP elsewhere.

namespace
{
	// For its scope, collects Archimedes' own warnings instead of printing them; every other
	// message goes on to the suite's handler, so a validation error still fails the test.
	class WarningCapture
	{
	public:
		WarningCapture()
			: m_scope([this](const acm::LogMessage& message)
					  {
				if (message.category != acm::LogCategory::Library || message.severity != acm::LogSeverity::Warning)
					return false;
				std::lock_guard<std::mutex> lock(m_mutex);
				m_warnings.emplace_back(message.text);
				return true; })
		{
		}

		// The warnings collected since the last call.
		std::vector<std::string> take()
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			return std::exchange(m_warnings, {});
		}

	private:
		std::mutex m_mutex;
		std::vector<std::string> m_warnings;
		acmtest::ScopedLogHandler m_scope; // last: installed after, and removed before, what it uses
	};

	// Exactly one warning since the last check, mentioning each of `parts`.
	void requireOneWarning(WarningCapture& capture, std::initializer_list<const char*> parts)
	{
		const std::vector<std::string> warnings = capture.take();
		for (const std::string& warning : warnings)
			UNSCOPED_INFO("warning: " << warning);
		REQUIRE(warnings.size() == 1);
		for (const char* part : parts)
			CHECK_THAT(warnings.front(), Catch::Matchers::ContainsSubstring(part));
	}

	void requireNoWarning(WarningCapture& capture)
	{
		const std::vector<std::string> warnings = capture.take();
		for (const std::string& warning : warnings)
			UNSCOPED_INFO("warning: " << warning);
		REQUIRE(warnings.empty());
	}

	// A device of this run's kind with every optional feature left off.
	acm::Device deviceWithoutOptionalFeatures(acm::Instance& instance)
	{
		acm::DeviceConfig config;
		config.optionalFeatures = acm::DeviceFeatures{};
		acm::Device device = instance.createDevice(acmtest::selectGraphicsOption(instance), config);
		REQUIRE(device.valid());
		const acm::DeviceFeatures& enabled = device.enabledFeatures();
		REQUIRE_FALSE(enabled.fillModeNonSolid);
		REQUIRE_FALSE(enabled.wideLines);
		REQUIRE_FALSE(enabled.samplerAnisotropy);
		REQUIRE_FALSE(enabled.sampleRateShading);
		return device;
	}

	constexpr uint32_t kSize = 32;
} // namespace

TEST_CASE("A pipeline warns once when it falls back from feature-gated state", "[acm][gpu][log]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	acmtest::requireInstance(instance);
	acm::Device device = deviceWithoutOptionalFeatures(instance);

	const acm::Extent2D extent{kSize, kSize};
	acm::Texture texture = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::RenderTarget target = device.createRenderTarget(texture, acm::RenderTargetConfig{acm::RenderTargetFinish::CopySrc});
	REQUIRE(target.valid());
	const acm::PipelineShaders shaders{device.createShader(acmtest::triangleVertSpirv()), device.createShader(acmtest::triangleFragSpirv())};
	REQUIRE(shaders.vertex.valid());
	REQUIRE(shaders.fragment.valid());

	WarningCapture capture;
	acm::PipelineConfig config;

	SECTION("state every device supports")
	{
		REQUIRE(device.createPipeline(shaders, target, config).valid());
		requireNoWarning(capture);
	}

	SECTION("PolygonMode::Line without fillModeNonSolid")
	{
		config.polygonMode = acm::PolygonMode::Line;
		acm::Pipeline pipeline = device.createPipeline(shaders, target, config);
		REQUIRE(pipeline.valid());
		requireOneWarning(capture, {"pipeline", "PolygonMode::Line", "fillModeNonSolid", "PolygonMode::Fill"});

		// The fallback was decided at creation: drawing with the pipeline logs nothing more.
		const acm::Error error = device.submitSync([&](acm::CommandBuffer& cmd)
												   {
			cmd.beginRendering(target);
			cmd.setViewportAndScissor(extent);
			cmd.bindPipeline(pipeline);
			cmd.draw(3);
			cmd.draw(3);
			cmd.endRendering(); });
		REQUIRE_FALSE(error);
		requireNoWarning(capture);
	}

	SECTION("a lineWidth other than 1 without wideLines")
	{
		config.lineWidth = 4.0f;
		REQUIRE(device.createPipeline(shaders, target, config).valid());
		requireOneWarning(capture, {"pipeline", "lineWidth 4", "wideLines", "lineWidth 1"});
	}

	SECTION("minSampleShading without sampleRateShading")
	{
		config.minSampleShading = 0.5f;
		REQUIRE(device.createPipeline(shaders, target, config).valid());
		requireOneWarning(capture, {"pipeline", "minSampleShading 0.5", "sampleRateShading", "ignored"});
	}

	SECTION("each fallback warns on its own")
	{
		config = acm::PipelineConfig::Wireframe(2.0f);
		config.minSampleShading = 1.0f;
		REQUIRE(device.createPipeline(shaders, target, config).valid());
		const std::vector<std::string> warnings = capture.take();
		for (const std::string& warning : warnings)
			UNSCOPED_INFO("warning: " << warning);
		REQUIRE(warnings.size() == 3);
		CHECK_THAT(warnings[0], Catch::Matchers::ContainsSubstring("fillModeNonSolid"));
		CHECK_THAT(warnings[1], Catch::Matchers::ContainsSubstring("wideLines"));
		CHECK_THAT(warnings[2], Catch::Matchers::ContainsSubstring("sampleRateShading"));
	}
}

TEST_CASE("A sampler without samplerAnisotropy falls back to isotropic filtering with a warning", "[acm][gpu][log]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	acmtest::requireInstance(instance);
	acm::Device device = deviceWithoutOptionalFeatures(instance);

	WarningCapture capture;
	REQUIRE(device.createSampler(16.0f).valid());
	requireOneWarning(capture, {"sampler", "maxAnisotropy 16", "samplerAnisotropy", "isotropic"});

	// Isotropic requests need no feature.
	REQUIRE(device.createSampler().valid());
	REQUIRE(device.createSampler(1.0f).valid());
	requireNoWarning(capture);
}

TEST_CASE("A mipmapped texture that cannot have mips warns and gets one level", "[acm][gpu][log]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	acmtest::requireInstance(instance);
	acm::Device device = instance.createDevice(acmtest::selectGraphicsOption(instance));
	REQUIRE(device.valid());

	acm::TextureConfig mipmapped;
	mipmapped.mipmapped = true;
	const acm::Extent2D extent{64, 64}; // a full chain is 7 levels

	WarningCapture capture;
	// upload() cannot fill a depth texture, so it never gets a chain. (The other branch,
	// a color format the device cannot blit with linear filtering, has no format to provoke
	// it on the devices the suite runs on.)
	acm::Texture depth = device.createTexture(acm::Format::D32_Sfloat, extent, mipmapped);
	REQUIRE(depth.valid());
	CHECK(depth.mipLevels() == 1);
	requireOneWarning(capture, {"texture", "mipmapped", "D32_Sfloat", "1 mip level instead of 7"});

	// R8G8B8A8_Unorm must support blits and linear filtering on every device, so it gets
	// its chain and no warning.
	acm::Texture color = device.createTexture(acm::Format::R8G8B8A8_Unorm, extent, mipmapped);
	REQUIRE(color.valid());
	CHECK(color.mipLevels() == 7);
	requireNoWarning(capture);
}

TEST_CASE("An offscreen render target warns when its sample count is clamped", "[acm][gpu][log]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	acmtest::requireInstance(instance);
	acm::Device device = instance.createDevice(acmtest::selectGraphicsOption(instance));
	REQUIRE(device.valid());
	if (device.maxSampleCount() == acm::SampleCount::Eight)
		SKIP("this device supports 8x MSAA, so the request is not clamped");

	const acm::Extent2D extent{kSize, kSize};
	acm::Texture clampedTexture = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	acm::Texture plainTexture = device.createTexture(acm::Format::B8G8R8A8_Unorm, extent);
	REQUIRE(clampedTexture.valid());
	REQUIRE(plainTexture.valid());

	WarningCapture capture;
	acm::RenderTarget clamped = device.createRenderTarget(clampedTexture, acm::RenderTargetConfig{acm::RenderTargetFinish::CopySrc, false, acm::SampleCount::Eight});
	REQUIRE(clamped.valid());
	requireOneWarning(capture, {"render target", "8x MSAA", "not supported", "using"});

	acm::RenderTarget plain = device.createRenderTarget(plainTexture, acm::RenderTargetConfig{acm::RenderTargetFinish::CopySrc, false, acm::SampleCount::One});
	REQUIRE(plain.valid());
	requireNoWarning(capture);
}

TEST_CASE("A swapchain warns once when its sample count is clamped", "[acm][gpu][log]")
{
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	acmtest::requireInstance(instance);
	const acm::Extent2D extent{64, 64};
	acm::Surface surface = instance.createHeadlessSurface(extent);
	if (!surface.valid())
		SKIP("headless surface unavailable");
	const acm::SurfaceOption option = acmtest::selectSurfaceOption(instance, surface);
	acm::Device device = instance.createDevice(option.device);
	REQUIRE(device.valid());
	if (device.maxSampleCount() == acm::SampleCount::Eight)
		SKIP("this device supports 8x MSAA, so the request is not clamped");

	WarningCapture capture;
	acm::SwapChain swapChain = device.createSwapChain(surface, option, acm::SwapChainConfig{extent, false, acm::SampleCount::Eight});
	if (!swapChain.valid())
		SKIP("driver does not support a headless swapchain");
	// One warning for the swapchain, not one per image's render target.
	requireOneWarning(capture, {"swapchain", "8x MSAA", "not supported"});

	// A rebuild keeps the same clamp and does not warn again.
	REQUIRE(swapChain.recreate());
	requireNoWarning(capture);
}
