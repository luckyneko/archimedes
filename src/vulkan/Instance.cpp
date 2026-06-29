#include "archimedes/vulkan/Instance.h"

#include "archimedes/acmDevice.h"
#include "archimedes/acmInstance.h"
#include "archimedes/acmSurface.h"
#include "archimedes/acmVersion.h"
#include "archimedes/vulkan/Convert.h"
#include "archimedes/vulkan/Device.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

const char* acm::vulkan::Instance::resultString(VkResult result)
{
	switch (result)
	{
		case VK_SUCCESS:
			return "VK_SUCCESS";
		case VK_ERROR_OUT_OF_HOST_MEMORY:
			return "VK_ERROR_OUT_OF_HOST_MEMORY";
		case VK_ERROR_OUT_OF_DEVICE_MEMORY:
			return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
		case VK_ERROR_INITIALIZATION_FAILED:
			return "VK_ERROR_INITIALIZATION_FAILED";
		case VK_ERROR_INCOMPATIBLE_DRIVER:
			return "VK_ERROR_INCOMPATIBLE_DRIVER";
		case VK_ERROR_EXTENSION_NOT_PRESENT:
			return "VK_ERROR_EXTENSION_NOT_PRESENT";
		case VK_ERROR_LAYER_NOT_PRESENT:
			return "VK_ERROR_LAYER_NOT_PRESENT";
		default:
			return "unknown VkResult";
	}
}

bool acm::vulkan::Instance::extensionAvailable(const std::vector<VkExtensionProperties>& extensions, const char* name)
{
	return std::find_if(extensions.begin(), extensions.end(), [name](const VkExtensionProperties& extension)
						{ return std::strcmp(name, extension.extensionName) == 0; }) != extensions.end();
}

bool acm::vulkan::Instance::layerAvailable(const std::vector<VkLayerProperties>& layers, const char* name)
{
	return std::find_if(layers.begin(), layers.end(), [name](const VkLayerProperties& layer)
						{ return std::strcmp(name, layer.layerName) == 0; }) != layers.end();
}

VkResult acm::vulkan::Instance::createDebugMessenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* createInfo, VkDebugUtilsMessengerEXT* messenger)
{
	auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
	return create ? create(instance, createInfo, nullptr, messenger) : VK_ERROR_EXTENSION_NOT_PRESENT;
}

void acm::vulkan::Instance::destroyDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger)
{
	auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
	if (destroy)
		destroy(instance, messenger, nullptr);
}

VKAPI_ATTR VkBool32 VKAPI_CALL acm::vulkan::Instance::validationCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void*)
{
	if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		std::fprintf(stderr, "validation layer [error]: %s\n", callbackData->pMessage);
	else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		std::fprintf(stderr, "validation layer [warn]: %s\n", callbackData->pMessage);
	return VK_FALSE;
}

acm::vulkan::Instance::Instance(const char* appName, const acm::Version& appVersion, const acm::InstanceConfig& config)
	: m_surfaces(*this)
{
	uint32_t loaderVersion = VK_API_VERSION_1_0;
	auto enumerateInstanceVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
	if (!enumerateInstanceVersion || enumerateInstanceVersion(&loaderVersion) != VK_SUCCESS || loaderVersion < RequiredAPIVersion)
	{
		m_error = acm::Error("Vulkan 1.3 loader required");
		return;
	}

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = appName;
	appInfo.applicationVersion = VK_MAKE_VERSION(appVersion.major, appVersion.minor, appVersion.patch);
	appInfo.pEngineName = "archimedes";
	appInfo.engineVersion = VK_MAKE_VERSION(acm::VERSION.major, acm::VERSION.minor, acm::VERSION.patch);
	appInfo.apiVersion = RequiredAPIVersion;

	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());
	std::vector<const char*> extensionNames;
	for (const VkExtensionProperties& extension : availableExtensions)
	{
		const std::string_view name(extension.extensionName);
		const bool surfaceExtension = name.size() >= 8 && name.compare(name.size() - 8, 8, "_surface") == 0;
		const bool moltenVKVendorExtension = name.rfind("VK_MVK_", 0) == 0;
		if (surfaceExtension && !moltenVKVendorExtension)
			extensionNames.push_back(extension.extensionName);
	}

	const bool portability = config.portability && extensionAvailable(availableExtensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
	if (portability)
		extensionNames.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

	const bool debugUtils = config.debug && extensionAvailable(availableExtensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	if (debugUtils)
		extensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
	if (config.validation && layerAvailable(availableLayers, "VK_LAYER_KHRONOS_validation"))
		m_layerNames.push_back("VK_LAYER_KHRONOS_validation");

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledLayerCount = uint32_t(m_layerNames.size());
	createInfo.ppEnabledLayerNames = m_layerNames.data();
	createInfo.enabledExtensionCount = uint32_t(extensionNames.size());
	createInfo.ppEnabledExtensionNames = extensionNames.data();
	if (portability)
		createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
	if (debugUtils)
	{
		debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debugCreateInfo.pfnUserCallback = validationCallback;
		createInfo.pNext = &debugCreateInfo;
	}

	VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
	if (result != VK_SUCCESS)
	{
		m_error = acm::Error(std::string("failed to create instance: ") + resultString(result));
		return;
	}

	if (debugUtils)
	{
		result = createDebugMessenger(m_instance, &debugCreateInfo, &m_debugMessenger);
		if (result != VK_SUCCESS)
		{
			m_error = acm::Error(std::string("failed to create debug messenger: ") + resultString(result));
			return;
		}
	}

	enumerateGPUs();
}

acm::vulkan::Instance::~Instance()
{
	m_surfaces.clear();
	if (m_debugMessenger)
		destroyDebugMessenger(m_instance, m_debugMessenger);
	if (m_instance)
		vkDestroyInstance(m_instance, nullptr);
}

void acm::vulkan::Instance::enumerateGPUs()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
	std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, physicalDevices.data());

	for (VkPhysicalDevice physicalDevice : physicalDevices)
	{
		VkPhysicalDeviceProperties2 properties = {};
		properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		vkGetPhysicalDeviceProperties2(physicalDevice, &properties);
		if (properties.properties.apiVersion < RequiredAPIVersion)
			continue;

		VkPhysicalDeviceVulkan13Features vulkan13Features = {};
		vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		VkPhysicalDeviceFeatures2 features = {};
		features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features.pNext = &vulkan13Features;
		vkGetPhysicalDeviceFeatures2(physicalDevice, &features);
		if (!vulkan13Features.synchronization2 || !vulkan13Features.dynamicRendering)
			continue;

		m_physicalDevices.push_back(physicalDevice);
		acm::GPU& gpu = m_gpus.emplace_back();
		gpu.index = uint32_t(m_gpus.size() - 1);
		gpu.apiVersion = {uint8_t(VK_API_VERSION_MAJOR(properties.properties.apiVersion)), uint8_t(VK_API_VERSION_MINOR(properties.properties.apiVersion)), uint16_t(VK_API_VERSION_PATCH(properties.properties.apiVersion))};
		gpu.name = properties.properties.deviceName;
		gpu.type = acm::vulkan::fromVk(properties.properties.deviceType);
		gpu.features.fillModeNonSolid = features.features.fillModeNonSolid == VK_TRUE;
		gpu.features.wideLines = features.features.wideLines == VK_TRUE;
		gpu.features.samplerAnisotropy = features.features.samplerAnisotropy == VK_TRUE;
		gpu.features.sampleRateShading = features.features.sampleRateShading == VK_TRUE;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties2> queueFamilies(queueFamilyCount);
		for (VkQueueFamilyProperties2& queueFamily : queueFamilies)
			queueFamily.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
		vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyCount, queueFamilies.data());
		gpu.queueFamilies.resize(queueFamilyCount);
		for (size_t queueIndex = 0; queueIndex < queueFamilies.size(); ++queueIndex)
		{
			const VkQueueFamilyProperties& properties = queueFamilies[queueIndex].queueFamilyProperties;
			acm::GPUQueueFamily& queue = gpu.queueFamilies[queueIndex];
			queue.index = uint32_t(queueIndex);
			queue.queueCount = properties.queueCount;
			queue.supportsGraphics = (properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
			queue.supportsCompute = (properties.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;
			queue.supportsTransfer = (properties.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0;
		}
	}

	if (m_gpus.empty())
		m_error = acm::Error("no Vulkan 1.3 physical device with synchronization2 and dynamicRendering available");
}

VkPhysicalDevice acm::vulkan::Instance::physicalDevice(uint32_t index) const
{
	return index < m_physicalDevices.size() ? m_physicalDevices[index] : VK_NULL_HANDLE;
}

acm::Surface acm::vulkan::Instance::createSurface(VkSurfaceKHR surface)
{
	auto inserted = m_surfaces.emplace([this, surface](acm::vulkan::Surface& resource)
									   { return resource.create(*this, surface); });
	if (!inserted.resource)
		return acm::Surface(acm::Error("failed to create surface"));
	return acm::Surface(inserted.resource, inserted.handle);
}

acm::Device acm::vulkan::Instance::createDevice(const acm::GPU& gpu, uint32_t queueIdx)
{
	auto device = std::make_unique<acm::vulkan::Device>(*this, gpu, queueIdx);
	return acm::Device(std::move(device));
}
