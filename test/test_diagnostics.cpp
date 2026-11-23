/*
 *  Created by LuckyNeko on 22/11/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/Platform.h"
#include "vk_test_helpers.h"

#include <archimedes/acmLog.h>
#include <archimedes/acmVulkanInterop.h>
#include <archimedes/archimedes.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <atomic>
#include <catch2/catch_all.hpp>
#include <cstring>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// An instance's log handler (InstanceConfig::logHandler, acmLog.h) and the suite's
// validation harness (test/main.cpp, vk_test_helpers.h). A sampler asked for anisotropy on
// a device created without that feature is the library warning these tests provoke: it
// fires on every device. The [validation] cases that need VK_LAYER_KHRONOS_validation SKIP
// without it; the rest run everywhere.

namespace
{
	// Used by the two cases that need a real validation error. Breaks
	// VUID-VkSamplerCreateInfo-anisotropyEnable-01070 (anisotropic filtering without the
	// samplerAnisotropy feature): a spec violation every driver tolerates.
	constexpr const char* AnisotropyVuid = "VUID-VkSamplerCreateInfo-anisotropyEnable-01070";

	void createAnisotropicSamplerWithoutFeature(const acm::Device& device)
	{
		VkSamplerCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		info.magFilter = VK_FILTER_NEAREST;
		info.minFilter = VK_FILTER_NEAREST;
		info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		info.anisotropyEnable = VK_TRUE;
		info.maxAnisotropy = 1.0f;
		info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

		const VkDevice vkDevice = acm::interop::device(device);
		VkSampler sampler = VK_NULL_HANDLE;
		if (vkCreateSampler(vkDevice, &info, nullptr, &sampler) == VK_SUCCESS)
			vkDestroySampler(vkDevice, sampler, nullptr);
	}
} // namespace

// -----------------------------------------------------------------------------
// The instance log handler
// -----------------------------------------------------------------------------

TEST_CASE("InstanceConfig starts with the default log handler", "[acm][log]")
{
	const acm::InstanceConfig config;
	REQUIRE(config.logHandler);
	using Function = void (*)(const acm::LogMessage&);
	const Function* target = config.logHandler.target<Function>();
	REQUIRE(target != nullptr);
	CHECK(*target == &acm::defaultLogHandler);

	CHECK(std::string(acm::logSeverityName(acm::LogSeverity::Warning)) == "warning");
	CHECK(std::string(acm::logCategoryName(acm::LogCategory::Validation)) == "validation");
}

TEST_CASE("An instance's log handler receives only that instance's messages", "[acm][gpu][log]")
{
	struct Received
	{
		std::mutex mutex;
		std::vector<std::string> warnings; // Library warnings, the only kind this test provokes
	};
	const auto handlerFor = [](Received& received)
	{
		return [&received](const acm::LogMessage& message)
		{
			if (message.category == acm::LogCategory::Library && message.severity == acm::LogSeverity::Warning)
			{
				std::lock_guard<std::mutex> lock(received.mutex);
				received.warnings.emplace_back(message.text);
			}
			else
				acmtest::logHandler(message); // a validation error still fails the test
		};
	};

	// Declared before the instances, which may call their handlers until they are destroyed.
	Received first;
	Received second;
	acm::InstanceConfig firstConfig;
	firstConfig.logHandler = handlerFor(first);
	acm::InstanceConfig secondConfig;
	secondConfig.logHandler = handlerFor(second);
	acm::Instance firstInstance("acm-tests", acm::Version{0, 1, 0}, firstConfig);
	acm::Instance secondInstance("acm-tests", acm::Version{0, 1, 0}, secondConfig);
	acmtest::requireInstance(firstInstance);
	acmtest::requireInstance(secondInstance);
	acm::Device firstDevice = acmtest::createDeviceWithoutOptionalFeatures(firstInstance);
	acm::Device secondDevice = acmtest::createDeviceWithoutOptionalFeatures(secondInstance);

	// A fallback on the first instance's device reaches the first handler alone.
	acm::Sampler sampler = firstDevice.createSampler(4.0f);
	REQUIRE(sampler.valid());
	REQUIRE(first.warnings.size() == 1);
	CHECK_THAT(first.warnings.front(), Catch::Matchers::ContainsSubstring("sampler"));
	CHECK_THAT(first.warnings.front(), Catch::Matchers::ContainsSubstring("maxAnisotropy 4"));
	CHECK(second.warnings.empty());

	sampler = secondDevice.createSampler(8.0f);
	REQUIRE(sampler.valid());
	CHECK(first.warnings.size() == 1);
	REQUIRE(second.warnings.size() == 1);
	CHECK_THAT(second.warnings.front(), Catch::Matchers::ContainsSubstring("maxAnisotropy 8"));
}

TEST_CASE("An empty or throwing log handler does not disturb the call that logged", "[acm][gpu][log]")
{
	// A generator, not two SECTIONs: the device helpers below hold a generator of their own,
	// and Catch2 crashes on a single-value generator that follows sibling sections (which is
	// what a machine with one device kind runs).
	const bool throwing = GENERATE(false, true);
	acm::InstanceConfig config;
	if (throwing)
		config.logHandler = [](const acm::LogMessage&)
		{ throw std::runtime_error("handler failure"); };
	else
		config.logHandler = nullptr; // discards every message
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0}, config);
	acmtest::requireInstance(instance);
	acm::Device device = acmtest::createDeviceWithoutOptionalFeatures(instance);

	acm::Sampler sampler;
	CHECK_NOTHROW(sampler = device.createSampler(4.0f));
	CHECK(sampler.valid());
}

TEST_CASE("An instance's log handler is called from every thread that logs", "[acm][gpu][log]")
{
	static constexpr int Threads = 8;
	static constexpr int SamplersPerThread = 50;

	std::atomic<int> warnings{0};
	acm::InstanceConfig config;
	config.logHandler = [&warnings](const acm::LogMessage& message)
	{
		if (message.category == acm::LogCategory::Library && message.severity == acm::LogSeverity::Warning)
			warnings.fetch_add(1, std::memory_order_relaxed);
		else
			acmtest::logHandler(message);
	};
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0}, config);
	acmtest::requireInstance(instance);
	acm::Device device = acmtest::createDeviceWithoutOptionalFeatures(instance);

	std::atomic<int> invalid{0};
	std::vector<std::thread> threads;
	for (int thread = 0; thread < Threads; ++thread)
		threads.emplace_back([&device, &invalid]
							 {
			for (int index = 0; index < SamplersPerThread; ++index)
				if (!device.createSampler(4.0f).valid())
					invalid.fetch_add(1, std::memory_order_relaxed); });
	for (std::thread& thread : threads)
		thread.join();

	CHECK(invalid.load() == 0);
	CHECK(warnings.load() == Threads * SamplersPerThread);
}

TEST_CASE("Instance warns when validation is requested but the layer is missing", "[acm][gpu][log]")
{
	std::vector<std::string> warnings;
	acm::InstanceConfig config;
	config.validation = true;
	config.logHandler = [&warnings](const acm::LogMessage& message)
	{
		// Creation logs on this thread; the rest goes to the harness as usual.
		if (message.category == acm::LogCategory::Library && message.severity == acm::LogSeverity::Warning)
			warnings.emplace_back(message.text);
		else
			acmtest::logHandler(message);
	};
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0}, config);
	acmtest::requireInstance(instance);

	const bool warned = std::any_of(warnings.begin(), warnings.end(), [](const std::string& text)
									{ return text.find("VK_LAYER_KHRONOS_validation") != std::string::npos; });
	CHECK(warned == !instance.validationEnabled());
}

// -----------------------------------------------------------------------------
// The validation harness
// -----------------------------------------------------------------------------

TEST_CASE("The harness records validation errors for its listener", "[acm][log][validation]")
{
	// Deliberate: handed straight to the suite's handler, which prints them like any other.
	acmtest::logHandler({acm::LogSeverity::Error, acm::LogCategory::Validation, "VUID-acm-harness-recorded", "deliberate error (harness self-test, recorded)"});
	acmtest::logHandler({acm::LogSeverity::Warning, acm::LogCategory::Validation, "VUID-acm-harness-warning", "deliberate warning (harness self-test, not recorded)"});
	acmtest::logHandler({acm::LogSeverity::Error, acm::LogCategory::Library, "acm-harness-library", "deliberate library error (harness self-test, not recorded)"});

	// Claimed here, so the listener does not fail this test.
	const std::vector<acmtest::LoggedError> errors = acmtest::takeValidationErrors();
	REQUIRE(errors.size() == 1);
	CHECK(errors.front().id == "VUID-acm-harness-recorded");
	CHECK(errors.front().text == "deliberate error (harness self-test, recorded)");
	CHECK(acmtest::takeValidationErrors().empty());
}

TEST_CASE("A validation error logged during a test case fails it", "[acm][log][validation][!shouldfail]")
{
	// The harness's listener fails this test case when it ends; [!shouldfail] makes that the
	// passing outcome, and a pass (the listener missed it) the failing one.
	acmtest::logHandler({acm::LogSeverity::Error, acm::LogCategory::Validation, "VUID-acm-harness-listener", "deliberate error (harness self-test, must fail this test)"});
}

TEST_CASE("The suite validates every instance whenever the layer is available", "[acm][gpu][validation]")
{
	// A default config: only the harness (ACM_VALIDATION) can turn validation on here.
	acm::Instance instance = acmtest::createInstance();
	acmtest::requireInstance(instance); // FAILs without the layer when validation is required

	// Asks for the layer outright, quietly, to learn whether it is installed.
	acm::InstanceConfig probeConfig;
	probeConfig.validation = true;
	probeConfig.logHandler = nullptr;
	if (!acm::Instance("acm-tests", acm::Version{0, 1, 0}, probeConfig).validationEnabled())
		SKIP("VK_LAYER_KHRONOS_validation is not installed: the suite runs unvalidated "
			 "(ARCHIMEDES_TEST_VALIDATION=REQUIRE makes that a failure)");
#ifdef ACM_TEST_VALIDATION_OFF
	SKIP("ARCHIMEDES_TEST_VALIDATION=OFF");
#endif
	if (acm::environmentVariable("ACM_VALIDATION") == std::optional<std::string>("0"))
		SKIP("ACM_VALIDATION=0 opts this run out");
	CHECK(instance.validationEnabled());
}

TEST_CASE("Validation-layer errors reach the instance's log handler", "[acm][gpu][validation]")
{
	struct Provoked
	{
		std::mutex mutex;
		std::vector<acm::LogSeverity> severities;
		std::vector<acm::LogCategory> categories;
	} provoked;
	acm::InstanceConfig config;
	config.validation = true;
	config.debug = true;
	config.logHandler = [&provoked](const acm::LogMessage& message)
	{
		// Claims only the provoked VUID; anything else still fails the test, and a missing
		// layer's warning stays off stderr.
		if (std::strcmp(message.id, AnisotropyVuid) == 0)
		{
			std::lock_guard<std::mutex> lock(provoked.mutex);
			provoked.severities.push_back(message.severity);
			provoked.categories.push_back(message.category);
		}
		else if (message.category != acm::LogCategory::Library)
			acmtest::logHandler(message);
	};
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0}, config);
	acmtest::requireInstance(instance);
	if (!instance.validationEnabled())
		SKIP("VK_LAYER_KHRONOS_validation is not installed");
	acm::Device device = acmtest::createDeviceWithoutOptionalFeatures(instance);

	createAnisotropicSamplerWithoutFeature(device);

	REQUIRE_FALSE(provoked.severities.empty());
	CHECK(provoked.severities.front() == acm::LogSeverity::Error);
	CHECK(provoked.categories.front() == acm::LogCategory::Validation);
}

TEST_CASE("A validation-layer error fails the test case that caused it", "[acm][gpu][validation][!shouldfail]")
{
	// No REQUIREs that could fail for another reason: under [!shouldfail] any failure passes,
	// so everything but the provoked error SKIPs instead.
	acm::InstanceConfig config;
	config.validation = true;
	config.debug = true;
	acm::Instance instance = acmtest::createInstance(config);
	if (!instance.valid() || !instance.validationEnabled())
		SKIP("VK_LAYER_KHRONOS_validation is not installed");
	acm::DeviceConfig deviceConfig;
	deviceConfig.optionalFeatures = acm::DeviceFeatures{};
	acm::Device device = instance.createDevice(acmtest::selectGraphicsOption(instance), deviceConfig);
	if (!device.valid() || device.enabledFeatures().samplerAnisotropy)
		SKIP("no device without samplerAnisotropy");

	createAnisotropicSamplerWithoutFeature(device);
	// The harness's listener fails this test case when it ends.
}
