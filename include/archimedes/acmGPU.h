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
	struct GPUQueueFamily
	{
		uint32_t index{0};
		uint32_t queueCount{0};
		bool supportsGraphics{false};
		bool supportsCompute{false};
		bool supportsTransfer{false};
	};

	// The curated subset of optional device features the renderer can make use of.
	// On a GPU it reports availability; on a Device (enabledFeatures()) it reports what
	// was actually turned on (the available subset). Extend as new features are wired.
	struct GPUFeatures
	{
		bool fillModeNonSolid{false};  // POLYGON_MODE_LINE / POINT — wireframe
		bool wideLines{false};		   // lineWidth > 1.0
		bool samplerAnisotropy{false}; // anisotropic texture filtering
		bool sampleRateShading{false}; // per-sample shading (MSAA inside primitives)
	};

	struct GPU
	{
		uint32_t index{0};
		std::string name;
		acm::Version apiVersion;
		acm::PhysicalDeviceType type{acm::PhysicalDeviceType::Other};
		std::vector<acm::GPUQueueFamily> queueFamilies;
		acm::GPUFeatures features; // what this GPU supports (of the curated set)
	};

	struct GPUSurfaceSupport
	{
		uint32_t gpuIndex{0};
		std::vector<bool> queueFamilySupportsPresent;
		std::vector<acm::SurfaceFormat> supportedFormats;
		std::vector<acm::PresentMode> supportedPresentModes;
		acm::SurfaceCapabilities capabilities;
	};
} // namespace acm
