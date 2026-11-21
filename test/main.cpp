/*
 *  Created by LuckyNeko on 02/07/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/acmLog.h"
#include "archimedes/acmRuntime.h"
#include "archimedes/Platform.h"
#include "vk_test_helpers.h"

#include <vulkan/vulkan.h>

#include <algorithm>
#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

// Under ThreadSanitizer (ARCHIMEDES_SANITIZE=thread) the suite reads its suppressions file
// (test/tsan.supp, for the uninstrumented CPU driver) without TSAN_OPTIONS, so ctest, test
// discovery and direct runs all apply it. A TSAN_OPTIONS from the environment still wins.
#if defined(ACM_TSAN_SUPPRESSIONS)
extern "C" __attribute__((visibility("default"), used)) const char* __tsan_default_options()
{
	return "suppressions=" ACM_TSAN_SUPPRESSIONS;
}
#endif

namespace
{
#if !defined(ACM_REQUIRE_VALIDATION) && !defined(ACM_TEST_VALIDATION_OFF)
	bool validationLayerInstalled()
	{
		uint32_t count = 0;
		if (vkEnumerateInstanceLayerProperties(&count, nullptr) != VK_SUCCESS)
			return false;
		std::vector<VkLayerProperties> layers(count);
		if (vkEnumerateInstanceLayerProperties(&count, layers.data()) < VK_SUCCESS)
			return false;
		layers.resize(count);
		return std::any_of(layers.begin(), layers.end(), [](const VkLayerProperties& layer)
						   { return std::strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0; });
	}
#endif

	// One failure message for every validation error a section logged, grouped by message
	// id (a frame loop repeats the same VUID every frame), in first-seen order.
	std::string describe(const std::vector<acmtest::LoggedError>& errors)
	{
		struct Group
		{
			const acmtest::LoggedError* first;
			size_t count;
		};
		std::vector<Group> groups;
		for (const acmtest::LoggedError& error : errors)
		{
			auto group = std::find_if(groups.begin(), groups.end(), [&error](const Group& candidate)
									  { return candidate.first->id == error.id; });
			if (group == groups.end())
				groups.push_back({&error, 1});
			else
				++group->count;
		}

		constexpr size_t MaxGroups = 8;
		std::ostringstream out;
		out << errors.size() << " Vulkan validation error" << (errors.size() == 1 ? "" : "s") << " logged during this test";
		const std::string device = acmtest::notedDevice();
		if (!device.empty())
			out << " (device: " << device << ")";
		for (size_t index = 0; index < groups.size() && index < MaxGroups; ++index)
		{
			const Group& group = groups[index];
			out << "\n  " << (group.first->id.empty() ? std::string("(no message id)") : group.first->id);
			if (group.count > 1)
				out << " (x" << group.count << ")";
			out << ": " << group.first->text;
		}
		if (groups.size() > MaxGroups)
			out << "\n  ... and " << groups.size() - MaxGroups << " more message ids";
		return out.str();
	}

	// Fails the running test case when a Vulkan validation error was logged during it, with
	// the messages. A message reaches validationLogHandler() synchronously on whichever thread
	// made the offending Vulkan call (a render thread, say), so the error is only recorded
	// there and reported here, on the test thread. Listeners see sectionEnded before the
	// reporters do and before Catch2 totals the test case, so the failure counts toward
	// this test case and is reported inside it.
	class ValidationListener : public Catch::EventListenerBase
	{
	public:
		using Catch::EventListenerBase::EventListenerBase;

		void testCasePartialStarting(const Catch::TestCaseInfo&, uint64_t) override
		{
			// Nothing logged between test cases belongs to this one.
			acmtest::takeValidationErrors();
			acmtest::noteDevice({});
		}

		void sectionEnded(const Catch::SectionStats&) override
		{
			const std::vector<acmtest::LoggedError> errors = acmtest::takeValidationErrors();
			if (errors.empty())
				return;
			try
			{
				FAIL_CHECK(describe(errors));
			}
			catch (...)
			{
				// FAIL_CHECK throws only once --abort's failure budget is spent; the run then
				// stops after this test case anyway, and nothing may escape a listener.
			}
		}
	};
} // namespace

CATCH_REGISTER_LISTENER(ValidationListener)

int main(int argc, char* argv[])
{
	acm::useStagedVulkanRuntime();

	// Validate every instance the suite creates, including those test files build
	// themselves: acm::Instance reads ACM_VALIDATION when it is created. Without the layer
	// the suite runs as before unless validation is required, in which case requireInstance()
	// fails. A value already in the environment wins, so ACM_VALIDATION=0 opts a run out.
#if defined(ACM_REQUIRE_VALIDATION)
	acm::setEnvDefault("ACM_VALIDATION", "1");
#elif !defined(ACM_TEST_VALIDATION_OFF)
	if (validationLayerInstalled())
		acm::setEnvDefault("ACM_VALIDATION", "1");
#endif
#if defined(ACM_TEST_SYNC_VALIDATION)
	// The validation layer's own setting (khronos_validation.validate_sync).
	acm::setEnvDefault("VK_KHRONOS_VALIDATION_VALIDATE_SYNC", "true");
#endif

	acm::setLogHandler(acmtest::validationLogHandler);
	return Catch::Session().run(argc, argv);
}
