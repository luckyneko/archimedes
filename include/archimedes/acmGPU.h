#pragma once

#include <vulkan/vulkan.h>
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
        VkPhysicalDevice device{ VK_NULL_HANDLE };
        VkPhysicalDeviceProperties properties;
        std::vector<acm::GPUQueueFamily> queueFamilies;
    };

    struct GPUSurfaceSupport
    {
        uint32_t gpuIndex{ 0 };
        std::vector<bool> queueFamilySupportsPresent;
        std::vector<VkSurfaceFormatKHR> supportedFormats;
        std::vector<VkPresentModeKHR> supportedPresentModes;
        VkSurfaceCapabilitiesKHR capabilities;
    };
}
