/*
 *  Created by LuckyNeko on 07/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include <archimedes/archimedes.h>

#include <catch2/catch_all.hpp>
#include <cstdint>
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
namespace acmtest
{
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
	inline void requireInstance(const acm::Instance& instance)
	{
		if (instance.valid())
			return;
#ifdef ACM_EXPECT_CPU_DEVICE
		FAIL("the CPU driver is staged but no Vulkan instance was created");
#endif
		SKIP("no Vulkan driver available");
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
			FAIL("the CPU driver is staged but no CPU device is available");
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
				return option;
			}
		}
		FAIL("no " << deviceKindName(kind) << " device"); // unreachable for a kind deviceKind() found
		return {};
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
				return option;
			}
		}
		SKIP("the " << deviceKindName(kind) << " device cannot present to a headless surface");
		return {};
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
	// a specific reason) and returns false, so a caller's `if (!build...) return;`
	// leaves a GPU-less run green.
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
		{
			SKIP("headless surface unavailable");
			return false;
		}

		const DeviceKind kind = policy == KindPolicy::TopRanked ? topRankedKind(out.instance) : deviceKind(out.instance);
		const acm::SurfaceOption option = selectSurfaceOption(out.instance, out.surface, kind);

		out.device = out.instance.createDevice(option.device);
		REQUIRE(out.device.valid());

		out.swapChain = out.device.createSwapChain(out.surface, option, acm::SwapChainConfig{acm::Extent2D{800, 600}});
		if (!out.swapChain.valid())
		{
			SKIP("driver does not support a headless swapchain");
			return false;
		}
		return true;
	}
} // namespace acmtest
