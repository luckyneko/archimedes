/*
 *  Created by LuckyNeko on 07/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmBackend.h"
#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmGPU.h"
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
		VkInstance vulkanInstance() const;

		// Factories
		acm::Surface createVulkanSurface(VkSurfaceKHR surface);
		// Creates a surface for offscreen ("headless") rendering without a visible
		// window, so the full swapchain/present path can run windowless. Uses
		// VK_EXT_headless_surface where available (e.g. MoltenVK) and otherwise an
		// off-screen platform window (Windows desktop ICDs lack that extension);
		// returns an invalid Surface where no mechanism exists. `extent` sizes the
		// backing window on platforms that need one and is otherwise unused — pass
		// the intended render size for consistent behaviour across platforms.
		acm::Surface createHeadlessSurface(acm::Extent2D extent = {1, 1});
		acm::Device createDevice(const acm::DeviceInfo& deviceInfo, uint32_t queueIndex);

		// Enumeration
		const std::vector<acm::DeviceInfo>& devices() const;

	private:
		std::unique_ptr<acm::backend::Instance> m;
		acm::Error m_error;
	};
} // namespace acm
