#include "archimedes/acmSwapChain.h"
#include "archimedes/acmDevice.h"
#include "archimedes/acmImage.h"
#include "archimedes/acmRenderTarget.h"
#include "archimedes/acmSurface.h"
#include <spdlog/spdlog.h>

struct acm::SwapChain::impl
{
    acm::Device device;
    VkSwapchainKHR swapChain{ VK_NULL_HANDLE };
    VkSurfaceFormatKHR format;
    VkPresentModeKHR mode;
    VkExtent2D extents;

    std::vector<acm::RenderTarget> renderTargets;

    ~impl()
    {
        if(swapChain)
            vkDestroySwapchainKHR(device.vkDevice(), swapChain, nullptr);
    }
};

acm::SwapChain::SwapChain(acm::Device device, acm::Surface surface, VkSurfaceFormatKHR format, VkPresentModeKHR presentMode)
: m()
{
    auto impl = std::make_shared<acm::SwapChain::impl>();
    impl->device = device;
    impl->format = format;
    impl->mode = presentMode;

    const auto& capabilities = surface.getGPUSupport()[impl->device.getGPU().index].capabilities;
    impl->extents = capabilities.currentExtent;
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
        imageCount = capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
    swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainCreateInfo.surface = surface.vkSurface();
    swapChainCreateInfo.minImageCount = imageCount;
    swapChainCreateInfo.imageFormat = impl->format.format;
    swapChainCreateInfo.imageColorSpace = impl->format.colorSpace;
    swapChainCreateInfo.imageExtent = impl->extents;
    swapChainCreateInfo.imageArrayLayers = 1;
    swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // Assume queue support presentation
    swapChainCreateInfo.queueFamilyIndexCount = 0;
    swapChainCreateInfo.pQueueFamilyIndices = (uint32_t*)0x72;
    swapChainCreateInfo.preTransform = capabilities.currentTransform;
    swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChainCreateInfo.presentMode = impl->mode;
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

    for(auto& image : vkImages)
    {
        // https://vulkan.lunarg.com/doc/view/1.0.30.0/windows/vkspec.chunked/ch29s06.html
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.flags = 0;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = swapChainCreateInfo.imageFormat;
        imageInfo.extent = {swapChainCreateInfo.imageExtent.width, swapChainCreateInfo.imageExtent.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = swapChainCreateInfo.imageArrayLayers;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = swapChainCreateInfo.imageUsage;
        imageInfo.sharingMode = swapChainCreateInfo.imageSharingMode;
        imageInfo.queueFamilyIndexCount = swapChainCreateInfo.queueFamilyIndexCount;
        imageInfo.pQueueFamilyIndices = swapChainCreateInfo.pQueueFamilyIndices;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        
        std::vector<acm::Image> acmImages;
        acmImages.push_back(acm::Image(impl->device, image, imageInfo));
        impl->renderTargets.push_back(acm::RenderTarget(impl->device, acmImages));
    }

    m = impl;
}

VkSwapchainKHR acm::SwapChain::vkSwapChain()
{
    return m->swapChain;
}

VkSurfaceFormatKHR acm::SwapChain::getFormat() const
{
    return m->format;
}

VkExtent2D acm::SwapChain::getExtents() const
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