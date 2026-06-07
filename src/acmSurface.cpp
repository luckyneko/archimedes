#include "archimedes/acmSurface.h"
#include "archimedes/acmInstance.h"
#include <spdlog/spdlog.h>

struct acm::Surface::impl
{
    acm::Instance instance;
    VkSurfaceKHR surface{ VK_NULL_HANDLE };
    std::vector<acm::GPUSurfaceSupport> gpuSupport;

    ~impl()
    {
        if(surface)
            vkDestroySurfaceKHR(instance.vkInstance(), surface, nullptr);
    }
};

acm::Surface::Surface(acm::Instance instance, VkSurfaceKHR surface)
: m()
{
    auto impl = std::make_shared<acm::Surface::impl>();
    impl->instance = instance;
    impl->surface = surface;

    // Load GPU support info
    const auto& availableGPUs = impl->instance.getAvailableGPUs();
    impl->gpuSupport.resize(availableGPUs.size());
    for(size_t gpuIdx = 0; gpuIdx < availableGPUs.size(); ++gpuIdx)
    {
        const auto& gpu = availableGPUs[gpuIdx];
        auto& gpuSupport = impl->gpuSupport[gpuIdx];
        gpuSupport.gpuIndex = gpu.index;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu.device, impl->surface, &gpuSupport.capabilities);

        uint32_t surfaceFormatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu.device, impl->surface, &surfaceFormatCount, nullptr);
        gpuSupport.supportedFormats.resize(surfaceFormatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu.device, impl->surface, &surfaceFormatCount, gpuSupport.supportedFormats.data());
    
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(gpu.device, impl->surface, &presentModeCount, nullptr);
        gpuSupport.supportedPresentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(gpu.device, impl->surface, &presentModeCount, gpuSupport.supportedPresentModes.data());

        gpuSupport.queueFamilySupportsPresent.resize(gpu.queueFamilies.size());
        for(const auto& queueFamily : gpu.queueFamilies)
        {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(gpu.device, queueFamily.index, impl->surface, &presentSupport);
            gpuSupport.queueFamilySupportsPresent[queueFamily.index] = (presentSupport == VK_TRUE);
        }
    }

    m = impl;
}

const std::vector<acm::GPUSurfaceSupport>& acm::Surface::getGPUSupport() const
{ 
    return m->gpuSupport;
}

VkSurfaceKHR acm::Surface::vkSurface()
{
    return m->surface;
}