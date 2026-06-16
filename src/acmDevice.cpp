#include "archimedes/acmDevice.h"
#include "archimedes/acmInstance.h"
#include <vulkan/vulkan.h>
#include <cstring>
#include <vector>

struct acm::Device::impl
{
    acm::Instance instance;
    acm::GPU gpu;
    uint32_t queueIdx{ 0 };
    VkDevice device{ VK_NULL_HANDLE };
    VkQueue queue{ VK_NULL_HANDLE };

    uint64_t currentFrame{ 0 };
    struct Pending
    {
        uint64_t frame{ 0 };
        std::function<void()> destroy;
    };
    std::vector<Pending> graveyard;

    ~impl()
    {
        if(device)
        {
            // The device is going away; nothing can still be in flight after we
            // wait, so flush every remaining deferred destroy before the device
            // itself. The lambdas only capture raw handles, so the live VkDevice
            // is valid right up until vkDestroyDevice below.
            vkDeviceWaitIdle(device);
            for(auto& entry : graveyard)
                entry.destroy();
            graveyard.clear();
            vkDestroyDevice(device, nullptr);
        }
    }
};

acm::Device::Device(acm::Instance instance, const acm::GPU& gpu, uint32_t queueIdx)
: m()
{
    auto impl = std::make_shared<acm::Device::impl>();
    impl->instance = instance;

    // Create logical device
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueIdx;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    const std::vector<const char*>& layerNames = instance.getLayerNames();
    std::vector<const char*> deviceExtensions =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    // Portability devices (e.g. MoltenVK on macOS) must enable
    // VK_KHR_portability_subset whenever they advertise it. The name macro
    // lives behind VK_ENABLE_BETA_EXTENSIONS, so match the literal instead.
    uint32_t deviceExtCount = 0;
    vkEnumerateDeviceExtensionProperties(gpu.device, nullptr, &deviceExtCount, nullptr);
    std::vector<VkExtensionProperties> availableDeviceExts(deviceExtCount);
    vkEnumerateDeviceExtensionProperties(gpu.device, nullptr, &deviceExtCount, availableDeviceExts.data());
    for(const auto& ext : availableDeviceExts)
    {
        if(strcmp(ext.extensionName, "VK_KHR_portability_subset") == 0)
        {
            deviceExtensions.push_back("VK_KHR_portability_subset");
            break;
        }
    }
    VkPhysicalDeviceFeatures deviceFeatures = {};

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.enabledLayerCount = uint32_t(layerNames.size());
    deviceCreateInfo.ppEnabledLayerNames = layerNames.data();
    deviceCreateInfo.enabledExtensionCount = uint32_t(deviceExtensions.size());;
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    if (vkCreateDevice(gpu.device, &deviceCreateInfo, nullptr, &impl->device) != VK_SUCCESS)
        return;
    vkGetDeviceQueue(impl->device, queueIdx, 0, &impl->queue);
    impl->gpu = gpu;
    impl->queueIdx = queueIdx;
    
    m = impl;
}

const acm::GPU& acm::Device::getGPU() const
{ 
    return m->gpu;
}
 
uint32_t acm::Device::getQueueIdx() const
{
    return m->queueIdx;
}
 
VkDevice acm::Device::vkDevice()
{
    return m->device;
}
 
VkQueue acm::Device::vkQueue()
{
    return m->queue;
}

void acm::Device::beginFrame()
{
    ++m->currentFrame;
}

uint64_t acm::Device::currentFrame() const
{
    return m->currentFrame;
}

void acm::Device::enqueueDestroy(std::function<void()> destroy)
{
    m->graveyard.push_back({ m->currentFrame, std::move(destroy) });
}

void acm::Device::collectGarbage(uint64_t completedFrame)
{
    auto& graveyard = m->graveyard;
    auto it = graveyard.begin();
    while(it != graveyard.end())
    {
        if(it->frame <= completedFrame)
        {
            it->destroy();
            it = graveyard.erase(it);
        }
        else
        {
            ++it;
        }
    }
}