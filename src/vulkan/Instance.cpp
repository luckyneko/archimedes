#include "archimedes/vulkan/Instance.h"

#include "archimedes/acmDevice.h"
#include "archimedes/acmSurface.h"
#include "archimedes/acmVersion.h"
#include "archimedes/vulkan/Convert.h"
#include "archimedes/vulkan/Device.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <regex>
#include <string>
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

#ifndef NDEBUG
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
#endif

acm::vulkan::Instance::Instance(const char* appName, const acm::Version& appVersion)
	: m_surfaces(*this)
{
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = appName;
	appInfo.applicationVersion = VK_MAKE_VERSION(appVersion.major, appVersion.minor, appVersion.patch);
	appInfo.pEngineName = "archimedes";
	appInfo.engineVersion = VK_MAKE_VERSION(acm::VERSION.major, acm::VERSION.minor, acm::VERSION.patch);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());
	std::vector<const char*> extensionNames;
	for (const VkExtensionProperties& extension : availableExtensions)
		if (std::regex_match(std::string(extension.extensionName), std::regex("VK_.+_surface")))
			extensionNames.push_back(extension.extensionName);

	const bool portability = extensionAvailable(availableExtensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
	if (portability)
	{
		extensionNames.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
		if (extensionAvailable(availableExtensions, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
			extensionNames.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
	}

#ifndef NDEBUG
	const bool debugUtils = extensionAvailable(availableExtensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	if (debugUtils)
		extensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
#ifndef NDEBUG
	if (layerAvailable(availableLayers, "VK_LAYER_KHRONOS_validation"))
		m_layerNames.push_back("VK_LAYER_KHRONOS_validation");
	else if (layerAvailable(availableLayers, "VK_LAYER_LUNARG_standard_validation"))
		m_layerNames.push_back("VK_LAYER_LUNARG_standard_validation");
#endif

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledLayerCount = uint32_t(m_layerNames.size());
	createInfo.ppEnabledLayerNames = m_layerNames.data();
	createInfo.enabledExtensionCount = uint32_t(extensionNames.size());
	createInfo.ppEnabledExtensionNames = extensionNames.data();
	if (portability)
		createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

#ifndef NDEBUG
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
	if (debugUtils)
	{
		debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debugCreateInfo.pfnUserCallback = validationCallback;
		createInfo.pNext = &debugCreateInfo;
	}
#endif

	VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
	if (result != VK_SUCCESS)
	{
		m_error = acm::Error(std::string("failed to create instance: ") + resultString(result));
		return;
	}

#ifndef NDEBUG
	if (debugUtils)
	{
		result = createDebugMessenger(m_instance, &debugCreateInfo, &m_debugMessenger);
		if (result != VK_SUCCESS)
		{
			m_error = acm::Error(std::string("failed to create debug messenger: ") + resultString(result));
			return;
		}
	}
#endif

	enumerateGPUs();
}

acm::vulkan::Instance::~Instance()
{
	m_surfaces.clear();
#ifndef NDEBUG
	if (m_debugMessenger)
		destroyDebugMessenger(m_instance, m_debugMessenger);
#endif
	if (m_instance)
		vkDestroyInstance(m_instance, nullptr);
}

void acm::vulkan::Instance::enumerateGPUs()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
	m_physicalDevices.resize(deviceCount);
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, m_physicalDevices.data());
	m_gpus.resize(deviceCount);

	for (size_t deviceIndex = 0; deviceIndex < m_physicalDevices.size(); ++deviceIndex)
	{
		VkPhysicalDevice physicalDevice = m_physicalDevices[deviceIndex];
		acm::GPU& gpu = m_gpus[deviceIndex];
		gpu.index = uint32_t(deviceIndex);
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);
		gpu.name = properties.deviceName;
		gpu.type = acm::vulkan::fromVk(properties.deviceType);

		VkPhysicalDeviceFeatures features;
		vkGetPhysicalDeviceFeatures(physicalDevice, &features);
		gpu.features.fillModeNonSolid = features.fillModeNonSolid == VK_TRUE;
		gpu.features.wideLines = features.wideLines == VK_TRUE;
		gpu.features.samplerAnisotropy = features.samplerAnisotropy == VK_TRUE;
		gpu.features.sampleRateShading = features.sampleRateShading == VK_TRUE;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());
		gpu.queueFamilies.resize(queueFamilyCount);
		for (size_t queueIndex = 0; queueIndex < queueFamilies.size(); ++queueIndex)
		{
			acm::GPUQueueFamily& queue = gpu.queueFamilies[queueIndex];
			queue.index = uint32_t(queueIndex);
			queue.queueCount = queueFamilies[queueIndex].queueCount;
			queue.supportsGraphics = (queueFamilies[queueIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
			queue.supportsCompute = (queueFamilies[queueIndex].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;
			queue.supportsTransfer = (queueFamilies[queueIndex].queueFlags & VK_QUEUE_TRANSFER_BIT) != 0;
		}
	}
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
