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
		bool fillModeNonSolid{false};  // POLYGON_MODE_LINE / POINT — wireframe
		bool wideLines{false};		   // lineWidth > 1.0
		bool samplerAnisotropy{false}; // anisotropic texture filtering
		bool sampleRateShading{false}; // per-sample shading (MSAA inside primitives)
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
		// Non-encoding (UNORM) surface formats first: the driver stores what a shader
		// writes without applying the linear->sRGB transfer function, so producers own
		// their encoding — a producer writes display-ready sRGB bytes (a GUI its native
		// colours as-is; linear-lit rendering encodes in-shader before write). The
		// colorspace stays SrgbNonlinear, so the compositor still reads those bytes as
		// sRGB. This is the safe default: it avoids the subtle double-encode washout an
		// auto-encoding (SRGB-format) surface inflicts on already-sRGB content (e.g. an
		// ImGui overlay). BGRA first — the native drawable format on Metal and on NVIDIA
		// Windows/Linux; channel order is a memory-layout detail the render/sample path
		// maps for you, transparent to logical-RGBA shader code. Only UNORM is listed, so
		// a surface exposing no UNORM format fails loudly rather than degrading to washout;
		// a consumer that wants hardware sRGB auto-encode passes its own preferences.
		std::vector<acm::SurfaceFormat> formats{
			{acm::Format::B8G8R8A8_Unorm, acm::ColorSpace::SrgbNonlinear},
			{acm::Format::R8G8B8A8_Unorm, acm::ColorSpace::SrgbNonlinear},
		};
		std::vector<acm::PresentMode> presentModes{
			acm::PresentMode::Fifo,
			acm::PresentMode::Mailbox,
			acm::PresentMode::FifoRelaxed,
			acm::PresentMode::Immediate,
		};
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
