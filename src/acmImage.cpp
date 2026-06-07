#include "archimedes/acmImage.h"
#include "archimedes/acmDevice.h"

struct acm::Image::impl
{
    acm::Device device;
    VkImage image{ VK_NULL_HANDLE };
    VkImageCreateInfo imageInfo{};
    bool externallyOwned{ false }; // Hack for SwapChain Images

    ~impl()
    {
        if(image && !externallyOwned)
            vkDestroyImage(device.vkDevice(), image, nullptr);
    }
};

acm::Image::Image(acm::Device device, VkImage image, VkImageCreateInfo info)
: m()
{
    m = std::make_shared<acm::Image::impl>();
    m->image = image;
    m->imageInfo = info;
    m->externallyOwned = true;
}

VkImageType acm::Image::type() const
{ 
    return m->imageInfo.imageType;
}

VkFormat acm::Image::format() const
{
    return m->imageInfo.format;
}

VkExtent3D acm::Image::extent() const
{
    return m->imageInfo.extent;
}

VkImage acm::Image::vkImage() const
{
    return m->image;
}

/*
acm::Image::Image()
: m->image(VK_NULL_HANDLE)
, m->imageInfo({})
{
    m->imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    m->imageInfo.pNext = nullptr;
    m->imageInfo.flags = 0; // Dunno
    m->imageInfo.imageType = VK_IMAGE_TYPE_2D;
    m->imageInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
    m->imageInfo.extent = {1024, 1024, 1};
    m->imageInfo.mipLevels = 1;
    m->imageInfo.arrayLayers = 1;
    m->imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    m->imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    m->imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    m->imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    m->imageInfo.initialLayout = VK_IMAGE_LAYOUT_GENERAL; // Should this change with flags
    m->imageInfo.queueFamilyIndexCount = 0;
    m->imageInfo.pQueueFamilyIndices = nullptr; // Only used in VK_SHARING_MODE_CONCURRENT

        if(!m->image)
    {
        if (vkCreateImage(m->device->vkDevice(), &m->imageInfo, nullptr, &m->image) != VK_SUCCESS)
        {
            spdlog::error("failed to create image!");
            return false;
        }
    }
    return true;
}
 */