/*
 *  Created by LuckyNeko on 07/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmBackend.h"
#include "archimedes/acmDeviceInfo.h"
#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmVersion.h"

#include <memory>
#include <vector>

namespace acm
{
	// Instance creation knobs. Portability is enabled by default so macOS/MoltenVK
	// devices enumerate without callers needing platform-specific setup.
	struct InstanceConfig
	{
		// Enumerate portability drivers such as MoltenVK when the extension is available.
		bool portability{true};
		// Enable an available standard validation layer.
		bool validation{false};
		// Enable VK_EXT_debug_utils and its diagnostic callback when available.
		bool debug{false};
		// Load the CPU driver staged beside the executable (<exe dir>/vulkan/) directly, so a
		// CPU device is available without driver environment variables or registration —
		// including in an elevated Windows process, where the loader ignores them. Needs a
		// loader with VK_LUNARG_direct_driver_loading (Vulkan SDK 1.3.230 or newer).
		bool cpuDriver{true};
	};

	// Unique owning root for Vulkan instance state. It enumerates physical devices and builds
	// Surfaces/Devices; every Surface and Device created from it must be reset before
	// the Instance is destroyed.
	class Instance
	{
	public:
		// Lifetime
		Instance();
		Instance(const char* appName, const acm::Version& appVer, const acm::InstanceConfig& config = {});
		Instance(const acm::Instance& other) = delete;
		Instance& operator=(const acm::Instance& other) = delete;
		Instance(acm::Instance&& other) noexcept;
		Instance& operator=(acm::Instance&& other) noexcept;
		~Instance();

		// State
		void reset();
		bool valid() const;
		acm::Error error() const;
		// Opaque backend handle for the Vulkan-interop seam (acmVulkanInterop.h). Not a
		// public native-handle API on its own — the raw VkInstance and VkSurfaceKHR
		// adoption live in acm::interop.
		acm::backend::Instance* backend() const;

		// Factories
		// Creates a surface for offscreen ("headless") rendering without a visible
		// window, so the full swapchain/present path can run windowless. Uses
		// VK_EXT_headless_surface where available (e.g. MoltenVK) and otherwise an
		// off-screen platform window (Windows desktop ICDs lack that extension);
		// returns an invalid Surface where no mechanism exists. `extent` sizes the
		// backing window on platforms that need one and is otherwise unused — pass
		// the intended render size for consistent behaviour across platforms.
		acm::Surface createHeadlessSurface(acm::Extent2D extent = {1, 1});
		acm::Device createDevice(const acm::DeviceInfo& deviceInfo, uint32_t queueFamily, const acm::DeviceConfig& config = {});
		acm::Device createDevice(const acm::DeviceOption& option, const acm::DeviceConfig& config = {});

		// Enumeration
		const std::vector<acm::DeviceInfo>& devices() const;
		// Graphics-capable options ranked by device kind; the no-argument form uses
		// DevicePreferences::Default() (hardware devices first, the CPU device last).
		std::vector<acm::DeviceOption> graphicsOptions() const;
		std::vector<acm::DeviceOption> graphicsOptions(const acm::DevicePreferences& preferences) const;
		// Device kind is the primary ranking; surface format and present mode rank within
		// each device. Forms without DevicePreferences use DevicePreferences::Default().
		std::vector<acm::SurfaceOption> surfaceOptions(const acm::Surface& surface) const;
		std::vector<acm::SurfaceOption> surfaceOptions(const acm::Surface& surface, const acm::SurfacePreferences& preferences) const;
		std::vector<acm::SurfaceOption> surfaceOptions(const std::vector<acm::Surface>& surfaces) const;
		std::vector<acm::SurfaceOption> surfaceOptions(const std::vector<acm::Surface>& surfaces, const acm::SurfacePreferences& preferences) const;
		std::vector<acm::SurfaceOption> surfaceOptions(const std::vector<acm::Surface>& surfaces, const acm::SurfacePreferences& surfacePreferences, const acm::DevicePreferences& devicePreferences) const;

	private:
		std::unique_ptr<acm::backend::Instance> m;
		acm::Error m_error;
	};
} // namespace acm
