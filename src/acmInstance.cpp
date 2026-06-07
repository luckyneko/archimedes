#include "archimedes/acmInstance.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cstring>
#include <regex>

namespace
{
#ifndef NDEBUG
    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
    {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        return (func != nullptr) ? func(instance, pCreateInfo, pAllocator, pDebugMessenger) : VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
    {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr)
            func(instance, debugMessenger, pAllocator);
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanValidationCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
    {
        if(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
            spdlog::error("validation layer: {0}", pCallbackData->pMessage);
        else if(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
            spdlog::warn("validation layer: {0}", pCallbackData->pMessage);
        else if(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
            spdlog::info("validation layer: {0}", pCallbackData->pMessage);
        else if(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
            spdlog::debug("validation layer: {0}", pCallbackData->pMessage);
        return VK_FALSE;
    }
#endif

    bool isExtensionAvailable(const std::vector<VkExtensionProperties>& availableExtentions, const char* extName)
    {
        auto iter = std::find_if(availableExtentions.begin(), availableExtentions.end(), 
            [extName](const VkExtensionProperties& extension) {return (strcmp(extName, extension.extensionName) == 0);});
        return iter != availableExtentions.end();
    }
    bool isLayerAvailable(const std::vector<VkLayerProperties>& availableLayers, const char* layerName)
    {
        auto iter = std::find_if(availableLayers.begin(), availableLayers.end(), 
            [layerName](const VkLayerProperties& layer) {return (strcmp(layerName, layer.layerName) == 0);});
        return iter != availableLayers.end();
    }
}

struct acm::Instance::impl
{
    VkInstance instance{ VK_NULL_HANDLE };
#ifndef NDEBUG
    VkDebugUtilsMessengerEXT debugMessenger{ VK_NULL_HANDLE };
#endif
    std::vector<VkExtensionProperties> availableExtensions;
    std::vector<VkLayerProperties> availableLayers;
    std::vector<const char*> extensionNames;
    std::vector<const char*> layerNames;
    std::vector<acm::GPU> gpus;

    ~impl()
    {
        #ifndef NDEBUG
        if(debugMessenger)
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        #endif
        if(instance)
            vkDestroyInstance(instance, nullptr);
    }
};

acm::Instance::Instance(const char* appName, const acm::Version& appVer)
{
    auto impl = std::make_shared<acm::Instance::impl>();

    // Create app info
    VkApplicationInfo appInfo {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = appName;
    appInfo.applicationVersion = VK_MAKE_VERSION(appVer.major, appVer.minor, appVer.patch);
    appInfo.pEngineName = "archimedes";
    appInfo.engineVersion = VK_MAKE_VERSION(acm::VERSION.major, acm::VERSION.minor, acm::VERSION.patch);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    // Find available extensions
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    impl->availableExtensions.resize(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, impl->availableExtensions.data());
    spdlog::debug("AvailableExtensions:");
    for(auto extension : impl->availableExtensions)
        spdlog::debug("  - {0}", extension.extensionName);

    // Select extensions
    for(auto& ext : impl->availableExtensions)
    {
        if(std::regex_match(std::string(ext.extensionName), std::regex("VK_.+_surface")))
            impl->extensionNames.emplace_back(ext.extensionName);
    }

    // Portability drivers (e.g. MoltenVK on macOS) require the portability
    // enumeration extension to be enabled and the matching create flag set,
    // otherwise vkCreateInstance returns VK_ERROR_INCOMPATIBLE_DRIVER. A no-op
    // on platforms whose ICDs don't advertise it.
    const bool portability = isExtensionAvailable(impl->availableExtensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    if(portability)
    {
        impl->extensionNames.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        if(isExtensionAvailable(impl->availableExtensions, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
            impl->extensionNames.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }
#ifndef NDEBUG
    if(isExtensionAvailable(impl->availableExtensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
        impl->extensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
    spdlog::debug("SelectedExtensions:");
    for(auto extension : impl->extensionNames)
        spdlog::debug("  - {0}", extension);

    // Find available layers
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    impl->availableLayers.resize(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, impl->availableLayers.data());
    spdlog::debug("AvailableLayers:");
    for(auto layer : impl->availableLayers)
        spdlog::debug("  - {0}", layer.layerName);

    // Select layers
#ifndef NDEBUG
    if(isLayerAvailable(impl->availableLayers, "VK_LAYER_KHRONOS_validation"))
        impl->layerNames.push_back("VK_LAYER_KHRONOS_validation");
    if(isLayerAvailable(impl->availableLayers, "VK_LAYER_LUNARG_standard_validation"))
        impl->layerNames.push_back("VK_LAYER_LUNARG_standard_validation");
#endif
    spdlog::debug("SelectedLayers:");
    for(auto layer : impl->layerNames)
        spdlog::debug("  - {0}", layer);

    // Create info
    VkInstanceCreateInfo instanceCreateinfo {};
    instanceCreateinfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateinfo.pApplicationInfo = &appInfo;
    instanceCreateinfo.enabledLayerCount = uint32_t(impl->layerNames.size());
    instanceCreateinfo.ppEnabledLayerNames = impl->layerNames.data();
    instanceCreateinfo.enabledExtensionCount = uint32_t(impl->extensionNames.size());
    instanceCreateinfo.ppEnabledExtensionNames = impl->extensionNames.data();
    if(portability)
        instanceCreateinfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

    // Debug info
#ifndef NDEBUG
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo {};
    debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugCreateInfo.pfnUserCallback = vulkanValidationCallback;
    instanceCreateinfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
#endif

    // Create Instance
    VkResult instanceRes = vkCreateInstance(&instanceCreateinfo, nullptr, &impl->instance);
    if (instanceRes != VK_SUCCESS)
    {
        spdlog::error("Failed to create instance: {0}", int(instanceRes));
        return;
    }

#ifndef NDEBUG
    // Create debug messenger
    VkResult debugMessengerRes = CreateDebugUtilsMessengerEXT(impl->instance, &debugCreateInfo, nullptr, &impl->debugMessenger);
    if (debugMessengerRes != VK_SUCCESS)
    {
        spdlog::error("Failed to set up debug messenger: {0}", int(debugMessengerRes));
        return;
    }
#endif

    // Populate GPU data
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(impl->instance, &deviceCount, nullptr);
    impl->gpus.resize(deviceCount);

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(impl->instance, &deviceCount, devices.data());
    for(size_t deviceIdx = 0; deviceIdx < devices.size(); ++deviceIdx)
    {
        // Device properties
        auto& gpu = impl->gpus[deviceIdx];
        gpu.device = devices[deviceIdx];
        gpu.index = uint32_t(deviceIdx);
        vkGetPhysicalDeviceProperties(gpu.device, &gpu.properties);

        // Load QueueFamilies
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(gpu.device, &queueFamilyCount, nullptr);
        gpu.queueFamilies.resize(queueFamilyCount);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(gpu.device, &queueFamilyCount, queueFamilies.data());

        // Parse QueueFamily data into acmQueueFamily
        for(size_t queueFamilyIdx = 0; queueFamilyIdx < queueFamilies.size(); ++queueFamilyIdx)
        {
            auto& gpuQueueFamily = gpu.queueFamilies[queueFamilyIdx];
            auto& queueFamily = queueFamilies[queueFamilyIdx];
            gpuQueueFamily.index = uint32_t(queueFamilyIdx);
            gpuQueueFamily.queueCount = queueFamily.queueCount;
            gpuQueueFamily.supportsGraphics = queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT;
            gpuQueueFamily.supportsCompute = queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT;
            gpuQueueFamily.supportsTransfer = queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT;
        }
    }

    // If all succeded, keep
    m = impl;
}

const std::vector<const char *>& acm::Instance::getLayerNames() const
{ 
    return m->layerNames;
}

const std::vector<acm::GPU>& acm::Instance::getAvailableGPUs() const
{ 
    return m->gpus;
}

VkInstance acm::Instance::vkInstance()
{ 
    return m->instance;
}