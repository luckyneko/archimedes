/*
 *  Created by LuckyNeko on 07/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include <archimedes/acmLog.h>
#include <archimedes/archimedes.h>

#include <catch2/catch_all.hpp>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

// Shared scaffolding for the [gpu] integration tests. Header-only (inline) so
// each test translation unit can include it without an extra link target. The
// windowless surface (and its Windows off-screen-window fallback) is a first-class
// acm::Instance::createHeadlessSurface() capability, so this stays backend-neutral
// — no raw Vulkan or platform headers.
//
// A test that selects its device through these helpers runs once per device kind the
// instance offers (a Catch2 generator): on the best hardware device, then on the CPU
// device. Only present kinds are generated, so a GPU-less machine runs each test once,
// on the CPU device, and reports it passed rather than skipped. When the CPU driver is
// staged (ACM_EXPECT_CPU_DEVICE) a missing CPU device means staging is broken and FAILs.
//
// Validation: test/main.cpp sets ACM_VALIDATION before any instance exists whenever the
// loader finds VK_LAYER_KHRONOS_validation, so every instance the suite creates — through
// these helpers or not — runs validated, and it installs validationLogHandler(). A Catch2
// listener in main.cpp then fails the test case during which a validation error was logged,
// with the messages in the failure. ARCHIMEDES_TEST_VALIDATION=REQUIRE (ACM_REQUIRE_VALIDATION)
// makes a missing layer FAIL requireInstance(); =OFF leaves instances as their configs ask.
namespace acmtest
{
	// A validation error the harness recorded.
	struct LoggedError
	{
		std::string id;
		std::string text;
	};

	// What the suite's log handler records for the listener. Never destroyed, so a message
	// logged during static destruction is still safe.
	struct ValidationCapture
	{
		std::mutex mutex;
		std::vector<LoggedError> errors;
		std::string device; // the device this test run selected, named in the failure
	};

	inline ValidationCapture& validationCapture()
	{
		static ValidationCapture* capture = new ValidationCapture;
		return *capture;
	}

	// The suite's log handler (installed by test/main.cpp): records every validation error
	// for the listener, then prints like the default handler. Thread-safe.
	inline void validationLogHandler(const acm::LogMessage& message)
	{
		if (message.category == acm::LogCategory::Validation && message.severity == acm::LogSeverity::Error)
		{
			ValidationCapture& capture = validationCapture();
			std::lock_guard<std::mutex> lock(capture.mutex);
			capture.errors.push_back({message.id, message.text});
		}
		acm::defaultLogHandler(message);
	}

	// The validation errors recorded since the last call, which the listener then no longer
	// sees. The listener calls it at each section's end; a test may call it to claim errors
	// it provoked on purpose.
	inline std::vector<LoggedError> takeValidationErrors()
	{
		ValidationCapture& capture = validationCapture();
		std::lock_guard<std::mutex> lock(capture.mutex);
		return std::exchange(capture.errors, {});
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

	// The device the running test selected (empty until a helper selects one).
	inline void noteDevice(std::string device)
	{
		ValidationCapture& capture = validationCapture();
		std::lock_guard<std::mutex> lock(capture.mutex);
		capture.device = std::move(device);
	}

	inline std::string notedDevice()
	{
		ValidationCapture& capture = validationCapture();
		std::lock_guard<std::mutex> lock(capture.mutex);
		return capture.device;
	}

	enum class DeviceKind
	{
		Hardware,
		Cpu,
	};

	inline const char* deviceKindName(DeviceKind kind)
	{
		return kind == DeviceKind::Cpu ? "CPU" : "hardware";
	}

	inline DeviceKind deviceKindOf(acm::PhysicalDeviceType type)
	{
		return type == acm::PhysicalDeviceType::Cpu ? DeviceKind::Cpu : DeviceKind::Hardware;
	}

	// SKIPs (or FAILs, when the CPU driver is staged) when no Vulkan instance could be made.
	// With validation required, an instance without the validation layer FAILs too.
	inline void requireInstance(const acm::Instance& instance)
	{
		if (!instance.valid())
		{
#ifdef ACM_EXPECT_CPU_DEVICE
			FAIL("the CPU driver is staged but no Vulkan instance was created");
#else
			SKIP("no Vulkan driver available");
#endif
		}
#ifdef ACM_REQUIRE_VALIDATION
		if (!instance.validationEnabled())
			FAIL("validation is required (ARCHIMEDES_TEST_VALIDATION=REQUIRE) but VK_LAYER_KHRONOS_validation "
				 "is not active: the loader did not find it (install it, or point VK_ADD_LAYER_PATH at its "
				 "manifest directory), or ACM_VALIDATION=0 is set");
#endif
	}

	// The device kind of this run. Every helper below shares this one generator, so a test
	// selecting its device through them runs once per kind the instance offers.
	inline DeviceKind deviceKind(const acm::Instance& instance)
	{
		bool hardware = false;
		bool cpu = false;
		for (const acm::DeviceOption& option : instance.graphicsOptions())
			(deviceKindOf(instance.devices()[option.deviceIndex].type) == DeviceKind::Cpu ? cpu : hardware) = true;
#ifdef ACM_EXPECT_CPU_DEVICE
		if (!cpu)
			FAIL("the CPU driver is staged but no CPU device is available (acm::Instance loads it "
				 "from <exe dir>/vulkan/)");
#endif
		std::vector<DeviceKind> kinds;
		if (hardware)
			kinds.push_back(DeviceKind::Hardware);
		if (cpu)
			kinds.push_back(DeviceKind::Cpu);
		if (kinds.empty())
			SKIP("no graphics-capable device");
		return GENERATE_COPY(from_range(kinds));
	}

	// The kind of the top-ranked graphics device (DevicePreferences::Default()), for
	// callers that run once on the device an application would pick (benchmarks).
	inline DeviceKind topRankedKind(const acm::Instance& instance)
	{
		const std::vector<acm::DeviceOption> options = instance.graphicsOptions();
		if (options.empty())
			SKIP("no graphics-capable device");
		return deviceKindOf(instance.devices()[options.front().deviceIndex].type);
	}

	// The best graphics option of `kind`.
	inline acm::DeviceOption selectGraphicsOption(const acm::Instance& instance, DeviceKind kind)
	{
		for (const acm::DeviceOption& option : instance.graphicsOptions())
		{
			const acm::DeviceInfo& info = instance.devices()[option.deviceIndex];
			if (deviceKindOf(info.type) == kind)
			{
				UNSCOPED_INFO("device: " << info.name << " (" << deviceKindName(kind) << ")");
				noteDevice(info.name + " (" + deviceKindName(kind) + ")");
				return option;
			}
		}
		FAIL("no " << deviceKindName(kind) << " device"); // unreachable for a kind deviceKind() found
	}

	// The best graphics option of this run's device kind (see deviceKind()).
	inline acm::DeviceOption selectGraphicsOption(const acm::Instance& instance)
	{
		return selectGraphicsOption(instance, deviceKind(instance));
	}

	// The device of this run's kind and its queue family index. The pointer is valid for
	// the lifetime of the instance.
	inline const acm::DeviceInfo* selectGraphicsDevice(const acm::Instance& instance, uint32_t& queueIndex)
	{
		const acm::DeviceOption option = selectGraphicsOption(instance);
		queueIndex = option.queueFamily;
		return &instance.devices()[option.deviceIndex];
	}

	// The best option for `surface` on a device of `kind`; SKIPs when that kind cannot
	// present to the surface (a capability gap, not a staging fault).
	inline acm::SurfaceOption selectSurfaceOption(const acm::Instance& instance, const acm::Surface& surface, DeviceKind kind)
	{
		for (const acm::SurfaceOption& option : instance.surfaceOptions(surface))
		{
			const acm::DeviceInfo& info = instance.devices()[option.device.deviceIndex];
			if (deviceKindOf(info.type) == kind)
			{
				UNSCOPED_INFO("device: " << info.name << " (" << deviceKindName(kind) << ")");
				noteDevice(info.name + " (" + deviceKindName(kind) + ")");
				return option;
			}
		}
		SKIP("the " << deviceKindName(kind) << " device cannot present to a headless surface");
	}

	// The best option for `surface` on this run's device kind (see deviceKind()).
	inline acm::SurfaceOption selectSurfaceOption(const acm::Instance& instance, const acm::Surface& surface)
	{
		return selectSurfaceOption(instance, surface, deviceKind(instance));
	}

	// How a helper chooses the device kind: once per kind present (tests), or only the
	// top-ranked device (benchmarks, whose history compares one kind against itself).
	enum class KindPolicy
	{
		EachPresent,
		TopRanked,
	};

	// The full headless stack (instance -> surface -> device -> swapchain) that
	// the pipeline/render-loop tests build on. On any missing layer it SKIPs (with
	// a specific reason), which ends the test case, so it returns only when the stack
	// is complete; callers keep `if (!build...) return;` as the documented guard.
	struct HeadlessStack
	{
		acm::Instance instance;
		acm::Surface surface;
		acm::Device device;
		acm::SwapChain swapChain;
	};

	inline bool buildHeadlessStack(HeadlessStack& out, KindPolicy policy = KindPolicy::EachPresent)
	{
		out.instance = acm::Instance("acm-tests", acm::Version{0, 1, 0});
		requireInstance(out.instance);

		out.surface = out.instance.createHeadlessSurface(acm::Extent2D{800, 600});
		if (!out.surface.valid())
			SKIP("headless surface unavailable");

		const DeviceKind kind = policy == KindPolicy::TopRanked ? topRankedKind(out.instance) : deviceKind(out.instance);
		const acm::SurfaceOption option = selectSurfaceOption(out.instance, out.surface, kind);

		out.device = out.instance.createDevice(option.device);
		REQUIRE(out.device.valid());

		out.swapChain = out.device.createSwapChain(out.surface, option, acm::SwapChainConfig{acm::Extent2D{800, 600}});
		if (!out.swapChain.valid())
			SKIP("driver does not support a headless swapchain");
		return true;
	}
} // namespace acmtest
