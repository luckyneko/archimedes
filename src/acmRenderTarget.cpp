#include "archimedes/acmRenderTarget.h"
#include "archimedes/acmDevice.h"
#include "archimedes/acmImage.h"
#include "acmVkConvert.h"
#include <vulkan/vulkan.h>
#include <spdlog/spdlog.h>

struct acm::RenderTarget::impl
{
    acm::Device device;
    std::vector<acm::Image> images;
    std::vector<VkImageView> imageViews;
    VkRenderPass renderPass{ VK_NULL_HANDLE };
    VkFramebuffer frameBuffer{ VK_NULL_HANDLE };

    ~impl()
    {
        if(!device.valid())
            return;

        // Defer teardown onto the device's frame-fenced queue. Enqueue order is
        // run order, so framebuffer -> render pass -> views, mirroring the safe
        // inline ordering.
        VkDevice dev = device.vkDevice();
        if(frameBuffer)
        {
            VkFramebuffer fb = frameBuffer;
            device.enqueueDestroy([dev, fb]{ vkDestroyFramebuffer(dev, fb, nullptr); });
        }
        if(renderPass)
        {
            VkRenderPass rp = renderPass;
            device.enqueueDestroy([dev, rp]{ vkDestroyRenderPass(dev, rp, nullptr); });
        }
        for(auto imageView : imageViews)
        {
            if(imageView)
                device.enqueueDestroy([dev, imageView]{ vkDestroyImageView(dev, imageView, nullptr); });
        }
    }
};

acm::RenderTarget::RenderTarget(acm::Device device, const std::vector<acm::Image>& images)
: m()
{
    auto impl = std::make_shared<acm::RenderTarget::impl>();
    impl->device = device;
    impl->images = images;

    // Create image views
    impl->imageViews.resize(impl->images.size());
    for (size_t i = 0; i < impl->images.size(); i++)
    {
        auto& image = images[i];
        VkImageViewCreateInfo imageViewInfo = {};
        imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewInfo.image = image.vkImage();
        imageViewInfo.viewType = acm::detail::toVkImageViewType(image.type());
        imageViewInfo.format = acm::detail::toVk(image.format());
        imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // How to handle this
        imageViewInfo.subresourceRange.baseMipLevel = 0;
        imageViewInfo.subresourceRange.levelCount = 1;
        imageViewInfo.subresourceRange.baseArrayLayer = 0;
        imageViewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(impl->device.vkDevice(), &imageViewInfo, nullptr, &impl->imageViews[i]) != VK_SUCCESS)
        {
            spdlog::error("failed to create image views!");
            return;
        }
    }

    // Create Render Pass
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = acm::detail::toVk(impl->images[0].format()); // Hack
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(impl->device.vkDevice(), &renderPassInfo, nullptr, &impl->renderPass) != VK_SUCCESS)
    {
        spdlog::error("failed to create render pass!");
        return;
    }

    // Frame buffer
    VkFramebufferCreateInfo framebufferInfo = {};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = impl->renderPass;
    framebufferInfo.attachmentCount = uint32_t(impl->imageViews.size());
    framebufferInfo.pAttachments = impl->imageViews.data();
    framebufferInfo.width = impl->images[0].extent().width;
    framebufferInfo.height = impl->images[0].extent().height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(impl->device.vkDevice(), &framebufferInfo, nullptr, &impl->frameBuffer) != VK_SUCCESS)
    {
        spdlog::error("failed to create framebuffer!");
        return;
    }

    m = impl;
}

VkRenderPass acm::RenderTarget::vkRenderPass()
{ 
    return m->renderPass;
}

VkFramebuffer acm::RenderTarget::vkFramebuffer() 
{ 
    return m->frameBuffer;
}