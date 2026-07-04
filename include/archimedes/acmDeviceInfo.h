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
