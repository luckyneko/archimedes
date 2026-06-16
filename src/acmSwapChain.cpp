#include "archimedes/acmSwapChain.h"
#include "archimedes/acmDevice.h"
#include "archimedes/acmImage.h"
#include "archimedes/acmRenderTarget.h"
#include "archimedes/acmSurface.h"
#include "acmVkConvert.h"
#include <vulkan/vulkan.h>
#include <spdlog/spdlog.h>
#include <algorithm>

struct acm::SwapChain::impl
{
    acm::Device device;
    VkSwapchainKHR swapChain{ VK_NULL_HANDLE };
    acm::SurfaceFormat format;
    acm::Extent2D extents;

    std::vector<acm::RenderTarget> renderTargets;

    ~impl()
    {
        if(!device.valid())
            return;

        // Drop the render targets first so their views/framebuffers/passes are
        // enqueued for destruction ahead of the swapchain that backs them; the
        // device's deferred queue then runs them in that (safe) order.
        renderTargets.clear();
        if(swapChain)
        {
            VkDevice dev = device.vkDevice();
            VkSwapchainKHR sc = swapChain;
            device.enqueueDestroy([dev, sc]{ vkDestroySwapchainKHR(dev, sc, nullptr); });
        }
    }
};

acm::SwapChain::SwapChain(acm::Device device, acm::Surface surface, acm::SurfaceFormat format, acm::PresentMode presentMode, acm::Extent2D desiredExtent)
: m()
{
    auto impl = std::make_shared<acm::SwapChain::impl>();
    impl->device = device;
    impl->format = format;

    const VkSurfaceFormatKHR vkFormat{ acm::detail::toVk(format.format), acm::detail::toVk(format.colorSpace) };
    const VkPresentModeKHR vkPresentMode = acm::detail::toVk(presentMode);

    // Query the authoritative surface capabilities straight from the device +
    // surface (they can change, e.g. on resize); the neutral view on Surface is
    // for consumers, this path needs the raw values (transform etc.).
    VkSurfaceCapabilitiesKHR capabilities{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(impl->device.getGPU().device, surface.vkSurface(), &capabilities);

    // A currentExtent of UINT32_MAX means the surface (e.g. a headless surface,
    // or some platforms) defers the size to the application: clamp the requested
    // extent to the surface's allowed range. Otherwise the surface dictates it.
    VkExtent2D vkExtents;
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        vkExtents = capabilities.currentExtent;
    }
    else
    {
        vkExtents.width = std::clamp(desiredExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        vkExtents.height = std::clamp(desiredExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }
    impl->extents = { vkExtents.width, vkExtents.height };
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
        imageCount = capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
    swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainCreateInfo.surface = surface.vkSurface();
    swapChainCreateInfo.minImageCount = imageCount;
    swapChainCreateInfo.imageFormat = vkFormat.format;
    swapChainCreateInfo.imageColorSpace = vkFormat.colorSpace;
    swapChainCreateInfo.imageExtent = vkExtents;
    swapChainCreateInfo.imageArrayLayers = 1;
    swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // Assume queue support presentation
    swapChainCreateInfo.queueFamilyIndexCount = 0;
    swapChainCreateInfo.pQueueFamilyIndices = (uint32_t*)0x72;
    swapChainCreateInfo.preTransform = capabilities.currentTransform;
    swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChainCreateInfo.presentMode = vkPresentMode;
    swapChainCreateInfo.clipped = VK_TRUE;
    swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(impl->device.vkDevice(), &swapChainCreateInfo, nullptr, &impl->swapChain) != VK_SUCCESS)
    {
        spdlog::error("failed to create swap chain!");
        return;
    }

    std::vector<VkImage> vkImages;
    vkGetSwapchainImagesKHR(impl->device.vkDevice(), impl->swapChain, &imageCount, nullptr);
    vkImages.resize(imageCount);
    vkGetSwapchainImagesKHR(impl->device.vkDevice(), impl->swapChain, &imageCount, vkImages.data());

    const acm::ImageDesc imageDesc{ acm::ImageType::e2D, impl->format.format, { vkExtents.width, vkExtents.height, 1 } };
    for(auto& image : vkImages)
    {
        std::vector<acm::Image> acmImages;
        acmImages.push_back(acm::Image(impl->device, image, imageDesc));
        impl->renderTargets.push_back(acm::RenderTarget(impl->device, acmImages));
    }

    m = impl;
}

VkSwapchainKHR acm::SwapChain::vkSwapChain()
{
    return m->swapChain;
}

acm::SurfaceFormat acm::SwapChain::getFormat() const
{
    return m->format;
}

acm::Extent2D acm::SwapChain::getExtents() const
{
    return m->extents;
}

size_t acm::SwapChain::getRenderTargetCount() const
{
    return m->renderTargets.size();
}

acm::RenderTarget acm::SwapChain::getRenderTarget(size_t idx) const
{
    return m->renderTargets[idx];
}