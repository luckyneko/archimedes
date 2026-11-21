/*
 *  Created by LuckyNeko on 22/11/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/Log.h"
#include "vk_test_helpers.h"

#include <archimedes/acmLog.h>
#include <archimedes/acmVulkanInterop.h>
#include <archimedes/archimedes.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <atomic>
#include <catch2/catch_all.hpp>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// The log hook (acmLog.h) and the suite's validation harness (test/main.cpp,
// vk_test_helpers.h). The [gpu][validation] cases that need VK_LAYER_KHRONOS_validation
// SKIP without it; the rest run everywhere.

namespace
{
	struct Captured
	{
		acm::LogSeverity severity;
		acm::LogCategory category;
		std::string id;
		std::string text;
	};

	Captured capture(const acm::LogMessage& message)
	{
		return {message.severity, message.category, message.id, message.text};
	}

	// For its scope, offers every message to `consume`; what it declines (returns false) goes
	// on to the handler it replaced — the harness's — so an unrelated validation error still
	// fails the test. Install and restore from the test thread only.
	class ScopedLogHandler
	{
	public:
		explicit ScopedLogHandler(std::function<bool(const acm::LogMessage&)> consume)
		{
			// Two steps, so the new handler holds the old one; nothing else logs meanwhile.
			m_previous = acm::setLogHandler({});
			acm::setLogHandler([consume = std::move(consume), previous = m_previous](const acm::LogMessage& message)
							   {
				if (!consume(message) && previous)
					previous(message); });
		}
		ScopedLogHandler(const ScopedLogHandler&) = delete;
		ScopedLogHandler& operator=(const ScopedLogHandler&) = delete;
		~ScopedLogHandler() { acm::setLogHandler(std::move(m_previous)); }

		// The handler this scope replaced.
		const acm::LogHandler& previous() const { return m_previous; }

	private:
		acm::LogHandler m_previous;
	};

	bool hasId(const acm::LogMessage& message, const char* id)
	{
		return std::strcmp(message.id, id) == 0;
	}

	// An instance whose "validation layer missing" warning, if any, is kept off stderr.
	acm::Instance quietInstance(const acm::InstanceConfig& config)
	{
		ScopedLogHandler quiet([](const acm::LogMessage& message)
							   { return message.category == acm::LogCategory::Library && message.severity == acm::LogSeverity::Warning; });
		return acm::Instance("acm-tests", acm::Version{0, 1, 0}, config);
	}

	// A device of this run's kind with no optional feature enabled, so samplerAnisotropy is off.
	acm::Device deviceWithoutAnisotropy(acm::Instance& instance)
	{
		acm::DeviceConfig config;
		config.optionalFeatures = acm::DeviceFeatures{};
		acm::Device device = instance.createDevice(acmtest::selectGraphicsOption(instance), config);
		REQUIRE(device.valid());
		REQUIRE_FALSE(device.enabledFeatures().samplerAnisotropy);
		return device;
	}

	// Breaks VUID-VkSamplerCreateInfo-anisotropyEnable-01070 (anisotropic filtering without the
	// samplerAnisotropy feature): a spec violation every driver tolerates.
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

	constexpr const char* AnisotropyVuid = "VUID-VkSamplerCreateInfo-anisotropyEnable-01070";
} // namespace

// -----------------------------------------------------------------------------
// The log hook
// -----------------------------------------------------------------------------

TEST_CASE("The log handler receives library messages with their severity and category", "[acm][log]")
{
	std::vector<Captured> captured;
	{
		ScopedLogHandler scope([&captured](const acm::LogMessage& message)
							   {
			if (std::strncmp(message.id, "acm-test", 8) != 0)
				return false;
			captured.push_back(capture(message));
			return true; });

		acm::logMessage(acm::LogSeverity::Verbose, acm::LogCategory::Library, "verbose text", "acm-test-1");
		acm::logMessage(acm::LogSeverity::Info, acm::LogCategory::Library, std::string("info text"), std::string("acm-test-2"));
		acm::logMessage(acm::LogSeverity::Warning, acm::LogCategory::Library, "warning text", "acm-test-3");
		acm::logMessage(acm::LogSeverity::Error, acm::LogCategory::Library, "error text", "acm-test-4");
		acm::logMessage(acm::LogSeverity::Warning, acm::LogCategory::Performance, "performance text", "acm-test-5");
		acm::logMessage(acm::LogSeverity::Info, acm::LogCategory::Vulkan, nullptr, "acm-test-6");
	}

	REQUIRE(captured.size() == 6);
	CHECK(captured[0].severity == acm::LogSeverity::Verbose);
	CHECK(captured[1].severity == acm::LogSeverity::Info);
	CHECK(captured[2].severity == acm::LogSeverity::Warning);
	CHECK(captured[3].severity == acm::LogSeverity::Error);
	for (size_t index = 0; index < 4; ++index)
		CHECK(captured[index].category == acm::LogCategory::Library);
	CHECK(captured[0].text == "verbose text");
	CHECK(captured[1].text == "info text");
	CHECK(captured[1].id == "acm-test-2");
	CHECK(captured[4].category == acm::LogCategory::Performance);
	CHECK(captured[5].category == acm::LogCategory::Vulkan);
	CHECK(captured[5].text.empty()); // a null text arrives empty, not null

	CHECK(std::string(acm::logSeverityName(acm::LogSeverity::Warning)) == "warning");
	CHECK(std::string(acm::logCategoryName(acm::LogCategory::Validation)) == "validation");
}

TEST_CASE("The category helpers log as the library", "[acm][log]")
{
	std::vector<Captured> captured;
	{
		// The helpers carry no id, so this scope claims every Library message while it lasts.
		ScopedLogHandler scope([&captured](const acm::LogMessage& message)
							   {
			if (message.category != acm::LogCategory::Library)
				return false;
			captured.push_back(capture(message));
			return true; });
		acm::logVerbose("v");
		acm::logInfo("i");
		acm::logWarning(std::string("w"));
		acm::logError("e");
	}

	REQUIRE(captured.size() == 4);
	const acm::LogSeverity expected[] = {acm::LogSeverity::Verbose, acm::LogSeverity::Info, acm::LogSeverity::Warning, acm::LogSeverity::Error};
	for (size_t index = 0; index < 4; ++index)
	{
		CHECK(captured[index].severity == expected[index]);
		CHECK(captured[index].id.empty());
	}
	CHECK(captured[2].text == "w");
}

TEST_CASE("setLogHandler returns the handler it replaces, so it can be restored", "[acm][log]")
{
	int outer = 0;
	int inner = 0;
	ScopedLogHandler scope([&outer](const acm::LogMessage& message)
						   {
		if (!hasId(message, "acm-test-restore"))
			return false;
		++outer;
		return true; });
	const auto send = []
	{ acm::logMessage(acm::LogSeverity::Info, acm::LogCategory::Library, "restore", "acm-test-restore"); };

	acm::LogHandler replaced = acm::setLogHandler([&inner](const acm::LogMessage& message)
												  {
		if (hasId(message, "acm-test-restore"))
			++inner; });
	REQUIRE(replaced); // the scope's handler
	send();
	CHECK(inner == 1);
	CHECK(outer == 0);

	acm::LogHandler installed = acm::setLogHandler(std::move(replaced));
	REQUIRE(installed); // the inner handler comes back
	send();
	CHECK(inner == 1);
	CHECK(outer == 1);

	// An empty handler discards everything until the previous one is restored.
	acm::LogHandler restored = acm::setLogHandler({});
	send();
	CHECK(outer == 1);
	acm::setLogHandler(std::move(restored));
	send();
	CHECK(outer == 2);
}

TEST_CASE("A throwing log handler does not escape logMessage", "[acm][log]")
{
	acm::LogHandler previous = acm::setLogHandler([](const acm::LogMessage&)
												  { throw std::runtime_error("handler failure"); });
	CHECK_NOTHROW(acm::logMessage(acm::LogSeverity::Info, acm::LogCategory::Library, "thrown away", "acm-test-throw"));
	acm::setLogHandler(std::move(previous));
}

TEST_CASE("Concurrent logging delivers every message while the handler is swapped", "[acm][log]")
{
	static constexpr int Threads = 8;
	static constexpr int MessagesPerThread = 2000;
	static constexpr int Swaps = 2000;

	std::atomic<int> delivered{0};
	ScopedLogHandler scope([](const acm::LogMessage&)
						   { return false; });
	const acm::LogHandler harness = scope.previous();
	// Every handler installed below counts this test's messages and forwards the rest.
	const auto counting = [&delivered, harness](const acm::LogMessage& message)
	{
		if (hasId(message, "acm-test-concurrent"))
			delivered.fetch_add(1, std::memory_order_relaxed);
		else if (harness)
			harness(message);
	};
	acm::setLogHandler(counting);

	std::atomic<bool> start{false};
	std::vector<std::thread> loggers;
	for (int thread = 0; thread < Threads; ++thread)
		loggers.emplace_back([&start]
							 {
			while (!start.load())
				std::this_thread::yield();
			for (int message = 0; message < MessagesPerThread; ++message)
				acm::logMessage(acm::LogSeverity::Verbose, acm::LogCategory::Library, "concurrent", "acm-test-concurrent"); });
	std::thread swapper([&start, &counting]
						{
		while (!start.load())
			std::this_thread::yield();
		for (int swap = 0; swap < Swaps; ++swap)
			acm::setLogHandler(counting); });

	start.store(true);
	for (std::thread& logger : loggers)
		logger.join();
	swapper.join();

	CHECK(delivered.load() == Threads * MessagesPerThread);
}

TEST_CASE("Instance warns when validation is requested but the layer is missing", "[acm][gpu][log]")
{
	std::mutex mutex;
	std::vector<std::string> warnings;
	acm::InstanceConfig config;
	config.validation = true;
	acm::Instance instance;
	{
		// Observe only; every message still reaches the harness.
		ScopedLogHandler scope([&](const acm::LogMessage& message)
							   {
			if (message.category == acm::LogCategory::Library && message.severity == acm::LogSeverity::Warning)
			{
				std::lock_guard<std::mutex> lock(mutex);
				warnings.emplace_back(message.text);
			}
			return false; });
		instance = acm::Instance("acm-tests", acm::Version{0, 1, 0}, config);
	}
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
	// Deliberate: printed to stderr by the harness like any other message.
	acm::logMessage(acm::LogSeverity::Error, acm::LogCategory::Validation, "deliberate error (harness self-test, recorded)", "VUID-acm-harness-recorded");
	acm::logMessage(acm::LogSeverity::Warning, acm::LogCategory::Validation, "deliberate warning (harness self-test, not recorded)", "VUID-acm-harness-warning");
	acm::logMessage(acm::LogSeverity::Error, acm::LogCategory::Library, "deliberate library error (harness self-test, not recorded)", "acm-harness-library");

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
	acm::logMessage(acm::LogSeverity::Error, acm::LogCategory::Validation, "deliberate error (harness self-test, must fail this test)", "VUID-acm-harness-listener");
}

TEST_CASE("The suite validates every instance whenever the layer is available", "[acm][gpu][validation]")
{
	// A default config: only the harness (ACM_VALIDATION) can turn validation on here.
	acm::Instance instance("acm-tests", acm::Version{0, 1, 0});
	acmtest::requireInstance(instance); // FAILs without the layer when validation is required

	acm::InstanceConfig config;
	config.validation = true;
	if (!quietInstance(config).validationEnabled())
		SKIP("VK_LAYER_KHRONOS_validation is not installed: the suite runs unvalidated "
			 "(ARCHIMEDES_TEST_VALIDATION=REQUIRE makes that a failure)");
#ifdef ACM_TEST_VALIDATION_OFF
	SKIP("ARCHIMEDES_TEST_VALIDATION=OFF");
#endif
	const char* forced = std::getenv("ACM_VALIDATION");
	if (forced && std::strcmp(forced, "0") == 0)
		SKIP("ACM_VALIDATION=0 opts this run out");
	CHECK(instance.validationEnabled());
}

TEST_CASE("Validation-layer errors reach the log hook as validation errors", "[acm][gpu][validation]")
{
	acm::InstanceConfig config;
	config.validation = true;
	config.debug = true;
	acm::Instance instance = quietInstance(config);
	acmtest::requireInstance(instance);
	if (!instance.validationEnabled())
		SKIP("VK_LAYER_KHRONOS_validation is not installed");
	acm::Device device = deviceWithoutAnisotropy(instance);

	std::mutex mutex;
	std::vector<Captured> captured;
	{
		// Claims only the provoked VUID; anything else still fails the test.
		ScopedLogHandler scope([&](const acm::LogMessage& message)
							   {
			if (!hasId(message, AnisotropyVuid))
				return false;
			std::lock_guard<std::mutex> lock(mutex);
			captured.push_back(capture(message));
			return true; });
		createAnisotropicSamplerWithoutFeature(device);
	}

	REQUIRE_FALSE(captured.empty());
	CHECK(captured.front().severity == acm::LogSeverity::Error);
	CHECK(captured.front().category == acm::LogCategory::Validation);
	CHECK_FALSE(captured.front().text.empty());
}

TEST_CASE("A validation-layer error fails the test case that caused it", "[acm][gpu][validation][!shouldfail]")
{
	// No REQUIREs that could fail for another reason: under [!shouldfail] any failure passes,
	// so everything but the provoked error SKIPs instead.
	acm::InstanceConfig config;
	config.validation = true;
	config.debug = true;
	acm::Instance instance = quietInstance(config);
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
