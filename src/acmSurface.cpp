#include "archimedes/acmSurface.h"
#include "archimedes/acmInstance.h"
#include "acmVkConvert.h"
#include <vulkan/vulkan.h>
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

        // Query raw Vulkan support, then translate into the backend-neutral view.
        VkSurfaceCapabilitiesKHR caps{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu.device, impl->surface, &caps);
        gpuSupport.capabilities.minImageCount = caps.minImageCount;
        gpuSupport.capabilities.maxImageCount = caps.maxImageCount;
        gpuSupport.capabilities.currentExtent = { caps.currentExtent.width, caps.currentExtent.height };
        gpuSupport.capabilities.minImageExtent = { caps.minImageExtent.width, caps.minImageExtent.height };
        gpuSupport.capabilities.maxImageExtent = { caps.maxImageExtent.width, caps.maxImageExtent.height };

        uint32_t surfaceFormatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu.device, impl->surface, &surfaceFormatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> rawFormats(surfaceFormatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu.device, impl->surface, &surfaceFormatCount, rawFormats.data());
        // Only surface formats/modes the neutral API can represent; the rest are
        // silently dropped (a driver lists many an app will never use).
        gpuSupport.supportedFormats.reserve(rawFormats.size());
        for(const auto& f : rawFormats)
        {
            acm::Format fmt;
            acm::ColorSpace colorSpace;
            if(acm::detail::tryFromVk(f.format, fmt) && acm::detail::tryFromVk(f.colorSpace, colorSpace))
                gpuSupport.supportedFormats.push_back({ fmt, colorSpace });
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(gpu.device, impl->surface, &presentModeCount, nullptr);
        std::vector<VkPresentModeKHR> rawModes(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(gpu.device, impl->surface, &presentModeCount, rawModes.data());
        gpuSupport.supportedPresentModes.reserve(rawModes.size());
        for(const auto& mode : rawModes)
        {
            acm::PresentMode pm;
            if(acm::detail::tryFromVk(mode, pm))
                gpuSupport.supportedPresentModes.push_back(pm);
        }

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