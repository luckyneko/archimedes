/*
 *  Created by LuckyNeko on 24/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/vulkan/Instance.h"

#include "archimedes/acmDevice.h"
#include "archimedes/acmInstance.h"
#include "archimedes/acmSurface.h"
#include "archimedes/acmVersion.h"
#include "archimedes/Log.h"
#include "archimedes/Platform.h"
#include "archimedes/vulkan/Convert.h"
#include "archimedes/vulkan/Device.h"
#include "archimedes/vulkan/PlatformSurface.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace acm::vulkan
{
	namespace
	{
		// The CPU driver's library, as acm_stage_vulkan_runtime stages it in <exe dir>/vulkan/.
#if defined(_WIN32)
		constexpr const char* CpuDriverLibrary = "vk_swiftshader.dll";
#elif defined(__APPLE__)
		constexpr const char* CpuDriverLibrary = "libvk_swiftshader.dylib";
#else
		constexpr const char* CpuDriverLibrary = "libvk_swiftshader.so";
#endif

		constexpr const char* ValidationLayer = "VK_LAYER_KHRONOS_validation";

		// ACM_VALIDATION set to anything but empty or "0" turns validation and the debug
		// messenger on for every instance, whatever its config says, so a test suite or a
		// shipped binary can be validated without a rebuild.
		bool validationForcedByEnvironment()
		{
			const std::optional<std::string> value = acm::environmentVariable("ACM_VALIDATION");
			return value && !value->empty() && *value != "0";
		}

		acm::LogSeverity logSeverityOf(VkDebugUtilsMessageSeverityFlagBitsEXT severity)
		{
			if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
				return acm::LogSeverity::Error;
			if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
				return acm::LogSeverity::Warning;
			if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
				return acm::LogSeverity::Info;
			return acm::LogSeverity::Verbose;
		}

		acm::LogCategory logCategoryOf(VkDebugUtilsMessageTypeFlagsEXT type)
		{
			if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
				return acm::LogCategory::Validation;
			if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
				return acm::LogCategory::Performance;
			return acm::LogCategory::Vulkan;
		}
	} // namespace

	// -----------------------------------------------------------------------------
	// Lifetime
	// -----------------------------------------------------------------------------

	Instance::Instance(const char* appName, const acm::Version& appVersion, const acm::InstanceConfig& config)
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

		// The staged CPU driver joins through VK_LUNARG_direct_driver_loading, not the loader's
		// own discovery, which ignores driver environment variables in an elevated process.
		// The loader cannot see a directly loaded driver before vkCreateInstance, so the
		// driver's own instance extensions (its surfaces) are merged in here, before any name
		// below points into this list.
		PFN_vkGetInstanceProcAddr cpuDriverProcAddr = nullptr;
		if (config.cpuDriver && extensionAvailable(availableExtensions, VK_LUNARG_DIRECT_DRIVER_LOADING_EXTENSION_NAME))
			cpuDriverProcAddr = loadCpuDriver(availableExtensions);

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

		const bool forced = validationForcedByEnvironment();
		const bool debugRequested = config.debug || forced;
		const bool debugUtils = debugRequested && extensionAvailable(availableExtensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		if (debugUtils)
			extensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		else if (debugRequested)
			acm::logWarning("instance: debug messages requested but " VK_EXT_DEBUG_UTILS_EXTENSION_NAME " is not available; continuing without them");

		if (cpuDriverProcAddr)
			extensionNames.push_back(VK_LUNARG_DIRECT_DRIVER_LOADING_EXTENSION_NAME);

		uint32_t layerCount = 0;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
		const bool validationRequested = config.validation || forced;
		m_validation = validationRequested && layerAvailable(availableLayers, ValidationLayer);
		if (m_validation)
			m_layerNames.push_back(ValidationLayer);
		else if (validationRequested)
			acm::logWarning(std::string("instance: validation requested but ") + ValidationLayer + " is not available; continuing without it");

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
			debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			debugCreateInfo.pfnUserCallback = validationCallback;
			createInfo.pNext = &debugCreateInfo;
		}

		// Inclusive: the CPU driver joins the drivers the loader discovers itself. Chained in
		// front so debugCreateInfo, reused for the messenger below, keeps an empty pNext.
		VkDirectDriverLoadingInfoLUNARG cpuDriverInfo = {};
		VkDirectDriverLoadingListLUNARG cpuDriverList = {};
		if (cpuDriverProcAddr)
		{
			cpuDriverInfo.sType = VK_STRUCTURE_TYPE_DIRECT_DRIVER_LOADING_INFO_LUNARG;
			cpuDriverInfo.pfnGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddrLUNARG>(cpuDriverProcAddr);
			cpuDriverList.sType = VK_STRUCTURE_TYPE_DIRECT_DRIVER_LOADING_LIST_LUNARG;
			cpuDriverList.pNext = createInfo.pNext;
			cpuDriverList.mode = VK_DIRECT_DRIVER_LOADING_MODE_INCLUSIVE_LUNARG;
			cpuDriverList.driverCount = 1;
			cpuDriverList.pDrivers = &cpuDriverInfo;
			createInfo.pNext = &cpuDriverList;
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

		enumerateDevices();
	}

	Instance::~Instance()
	{
		m_surfaces.clear();
		if (m_debugMessenger)
			destroyDebugMessenger(m_instance, m_debugMessenger);
		if (m_instance)
			vkDestroyInstance(m_instance, nullptr);
	}

	// -----------------------------------------------------------------------------
	// Enumeration
	// -----------------------------------------------------------------------------

	VkPhysicalDevice Instance::physicalDevice(uint32_t index) const
	{
		return index < m_physicalDevices.size() ? m_physicalDevices[index] : VK_NULL_HANDLE;
	}

	// -----------------------------------------------------------------------------
	// Factories
	// -----------------------------------------------------------------------------

	acm::Surface Instance::createVulkanSurface(VkSurfaceKHR surface)
	{
		m_surfaces.collectGarbage();
		auto inserted = m_surfaces.emplace([this, surface]
										   { return Surface(*this, surface); });
		if (!inserted.valid())
			return acm::Surface(inserted.error ? std::move(inserted.error) : acm::Error("failed to create surface"));
		return acm::Surface(std::move(inserted.resource));
	}

	acm::Surface Instance::createHeadlessSurface(acm::Extent2D extent)
	{
		HeadlessSurface headless = acm::vulkan::createHeadlessSurface(m_instance, extent);
		if (headless.surface == VK_NULL_HANDLE)
			return acm::Surface(acm::Error("headless surface is unavailable on this platform/driver"));

		m_surfaces.collectGarbage();
		const VkSurfaceKHR surface = headless.surface;
		auto inserted = m_surfaces.emplace([this, surface, window = std::move(headless.window)]() mutable
										   { return Surface(*this, surface, std::move(window)); });
		if (!inserted.valid())
			return acm::Surface(inserted.error ? std::move(inserted.error) : acm::Error("failed to create headless surface"));
		return acm::Surface(std::move(inserted.resource));
	}

	acm::Device Instance::createDevice(const acm::DeviceInfo& deviceInfo, uint32_t queueFamily, const acm::DeviceConfig& config)
	{
		auto device = std::make_unique<Device>(*this, deviceInfo, queueFamily, config);
		return acm::Device(std::move(device));
	}

	// -----------------------------------------------------------------------------
	// Internals
	// -----------------------------------------------------------------------------

	const char* Instance::resultString(VkResult result)
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

	bool Instance::extensionAvailable(const std::vector<VkExtensionProperties>& extensions, const char* name)
	{
		return std::find_if(extensions.begin(), extensions.end(), [name](const VkExtensionProperties& extension)
							{ return std::strcmp(name, extension.extensionName) == 0; }) != extensions.end();
	}

	bool Instance::layerAvailable(const std::vector<VkLayerProperties>& layers, const char* name)
	{
		return std::find_if(layers.begin(), layers.end(), [name](const VkLayerProperties& layer)
							{ return std::strcmp(name, layer.layerName) == 0; }) != layers.end();
	}

	VkResult Instance::createDebugMessenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* createInfo, VkDebugUtilsMessengerEXT* messenger)
	{
		auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
		return create ? create(instance, createInfo, nullptr, messenger) : VK_ERROR_EXTENSION_NOT_PRESENT;
	}

	void Instance::destroyDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger)
	{
		auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
		if (destroy)
			destroy(instance, messenger, nullptr);
	}

	VKAPI_ATTR VkBool32 VKAPI_CALL Instance::validationCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void*)
	{
		// Relayed with its own severity; the category says whether it is a validation report,
		// a performance warning or a general loader/layer/driver message.
		if (callbackData)
			acm::logMessage(logSeverityOf(severity), logCategoryOf(type), callbackData->pMessage, callbackData->pMessageIdName);
		// VK_FALSE lets the call proceed, as the spec requires of an application callback.
		return VK_FALSE;
	}

	PFN_vkGetInstanceProcAddr Instance::loadCpuDriver(std::vector<VkExtensionProperties>& extensions)
	{
		const std::filesystem::path directory = executableDirectory();
		if (directory.empty())
			return nullptr;
		PlatformLibrary library(directory / "vulkan" / CpuDriverLibrary);
		auto procAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(library.symbol("vk_icdGetInstanceProcAddr"));
		auto enumerate = procAddr ? reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(procAddr(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties")) : nullptr;
		uint32_t count = 0;
		if (!enumerate || enumerate(nullptr, &count, nullptr) != VK_SUCCESS)
			return nullptr;
		std::vector<VkExtensionProperties> driverExtensions(count);
		if (enumerate(nullptr, &count, driverExtensions.data()) != VK_SUCCESS)
			return nullptr;
		driverExtensions.resize(count);

		for (const VkExtensionProperties& extension : driverExtensions)
			if (!extensionAvailable(extensions, extension.extensionName))
				extensions.push_back(extension);
		m_cpuDriver = std::move(library);
		return procAddr;
	}

	void Instance::enumerateDevices()
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
			acm::DeviceInfo& device = m_devices.emplace_back();
			device.index = uint32_t(m_devices.size() - 1);
			device.apiVersion = {uint8_t(VK_API_VERSION_MAJOR(properties.properties.apiVersion)), uint8_t(VK_API_VERSION_MINOR(properties.properties.apiVersion)), uint16_t(VK_API_VERSION_PATCH(properties.properties.apiVersion))};
			device.name = properties.properties.deviceName;
			device.type = fromVk(properties.properties.deviceType);
			device.features.fillModeNonSolid = features.features.fillModeNonSolid == VK_TRUE;
			device.features.wideLines = features.features.wideLines == VK_TRUE;
			device.features.samplerAnisotropy = features.features.samplerAnisotropy == VK_TRUE;
			device.features.sampleRateShading = features.features.sampleRateShading == VK_TRUE;
			device.features.fragmentStoresAndAtomics = features.features.fragmentStoresAndAtomics == VK_TRUE;

			uint32_t queueFamilyCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyCount, nullptr);
			std::vector<VkQueueFamilyProperties2> queues(queueFamilyCount);
			for (VkQueueFamilyProperties2& queueFamily : queues)
				queueFamily.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
			vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyCount, queues.data());
			device.queues.resize(queueFamilyCount);
			for (size_t queueIndex = 0; queueIndex < queues.size(); ++queueIndex)
			{
				const VkQueueFamilyProperties& family = queues[queueIndex].queueFamilyProperties;
				acm::QueueInfo& queue = device.queues[queueIndex];
				queue.family = uint32_t(queueIndex);
				queue.count = family.queueCount;
				queue.graphics = (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
				queue.compute = (family.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;
				queue.transfer = (family.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0;
			}
		}

		if (m_devices.empty())
			m_error = acm::Error("no Vulkan 1.3 physical device with synchronization2 and dynamicRendering available");
	}

} // namespace acm::vulkan
