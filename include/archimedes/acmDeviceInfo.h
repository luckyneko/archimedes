/*
 *  Created by LuckyNeko on 07/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmTypes.h"
#include "archimedes/acmVersion.h"

#include <string>
#include <vector>

namespace acm
{
	struct QueueInfo
	{
		uint32_t family{0};
		uint32_t count{0};
		bool graphics{false};
		bool compute{false};
		bool transfer{false};
	};

	// The curated subset of optional device features the renderer can make use of.
	// On a DeviceInfo it reports availability; on a Device (enabledFeatures()) it reports what
	// was actually turned on (the available subset). Extend as new features are wired.
	struct DeviceFeatures
	{
		static DeviceFeatures AllKnown() { return {true, true, true, true}; }

		bool fillModeNonSolid{false};  // POLYGON_MODE_LINE / POINT — wireframe
		bool wideLines{false};		   // lineWidth > 1.0
		bool samplerAnisotropy{false}; // anisotropic texture filtering
		bool sampleRateShading{false}; // per-sample shading (MSAA inside primitives)
	};

	struct DeviceConfig
	{
		// Required features must be available or device creation fails with an Error.
		acm::DeviceFeatures requiredFeatures;
		// Optional features are enabled when available. The default preserves the
		// original behavior: enable every curated feature the selected device supports.
		acm::DeviceFeatures optionalFeatures{acm::DeviceFeatures::AllKnown()};
	};

	struct DeviceInfo
	{
		uint32_t index{0};
		std::string name;
		acm::Version apiVersion;
		acm::PhysicalDeviceType type{acm::PhysicalDeviceType::Other};
		std::vector<acm::QueueInfo> queues;
		acm::DeviceFeatures features; // what this physical device supports (of the curated set)
	};

	struct DeviceOption
	{
		uint32_t deviceIndex{0};
		uint32_t queueFamily{0};
	};

	struct SurfacePreferences
	{
		// Empty means "no ranking preference"; surfaceOptions(surface) uses Default()
		// explicitly. Provide only the axes you want to rank.
		std::vector<acm::SurfaceFormat> formats;
		std::vector<acm::PresentMode> presentModes;

		// Safe general-purpose choice for mixed GUI and shader-rendered content.
		static SurfacePreferences Default()
		{
			// Non-encoding (UNORM) surface formats first: the driver stores what a shader
			// writes without applying the linear->sRGB transfer function, so producers own
			// their encoding. FIFO is the portable vsynced present mode.
			SurfacePreferences preferences;
			preferences.formats = {
				{acm::Format::B8G8R8A8_Unorm, acm::ColorSpace::SrgbNonlinear},
				{acm::Format::R8G8B8A8_Unorm, acm::ColorSpace::SrgbNonlinear},
			};
			preferences.presentModes = {
				acm::PresentMode::Fifo,
				acm::PresentMode::Mailbox,
				acm::PresentMode::FifoRelaxed,
				acm::PresentMode::Immediate,
			};
			return preferences;
		}

		// Prefer hardware sRGB attachment encoding for linear-color renderers.
		static SurfacePreferences HardwareSrgb()
		{
			SurfacePreferences preferences = Default();
			preferences.formats = {
				{acm::Format::B8G8R8A8_Srgb, acm::ColorSpace::SrgbNonlinear},
				{acm::Format::R8G8B8A8_Srgb, acm::ColorSpace::SrgbNonlinear},
			};
			return preferences;
		}

		// Prefer lower input latency over guaranteed vsync pacing.
		static SurfacePreferences LowLatency()
		{
			SurfacePreferences preferences = Default();
			preferences.presentModes = {
				acm::PresentMode::Mailbox,
				acm::PresentMode::Immediate,
				acm::PresentMode::FifoRelaxed,
				acm::PresentMode::Fifo,
			};
			return preferences;
		}
	};

	struct SurfaceOption
	{
		acm::DeviceOption device;
		acm::SurfaceFormat format;
		acm::PresentMode presentMode{acm::PresentMode::Fifo};
		acm::SurfaceCapabilities capabilities;
	};

	struct SurfaceDeviceSupport
	{
		uint32_t deviceIndex{0};
		std::vector<bool> queuePresentSupport;
		std::vector<acm::SurfaceFormat> formats;
		std::vector<acm::PresentMode> presentModes;
		acm::SurfaceCapabilities capabilities;
	};
} // namespace acm
