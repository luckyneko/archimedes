#pragma once

#include "archimedes/acmTypes.h"
#include "archimedes/acmVkFwd.h"
#include <string>
#include <vector>

namespace acm
{
	struct GPUQueueFamily
	{
		uint32_t index{ 0 };
		uint32_t queueCount{ 0 };
		bool supportsGraphics{ false };
		bool supportsCompute{ false };
		bool supportsTransfer{ false };
	};

	struct GPU
	{
		uint32_t index{ 0 };
		std::string name;
		acm::PhysicalDeviceType type{ acm::PhysicalDeviceType::Other };
		// Raw backend handle, opaque unless the caller also includes <vulkan/vulkan.h>.
		VkPhysicalDevice device{ nullptr };
		std::vector<acm::GPUQueueFamily> queueFamilies;
	};

	struct GPUSurfaceSupport
	{
		uint32_t gpuIndex{ 0 };
		std::vector<bool> queueFamilySupportsPresent;
		std::vector<acm::SurfaceFormat> supportedFormats;
		std::vector<acm::PresentMode> supportedPresentModes;
		acm::SurfaceCapabilities capabilities;
	};
}
