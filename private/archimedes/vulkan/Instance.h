/*
 *  Created by LuckyNeko on 24/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmDeviceInfo.h"
#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmLog.h"
#include "archimedes/acmVersion.h"
#include "archimedes/PlatformLibrary.h"
#include "archimedes/ResourcePool.h"
#include "archimedes/vulkan/Resources.h"

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace acm::vulkan
{
	// Move-only VkInstance owner. Enumerates physical devices and owns the stable pool
	// for platform surfaces created from this instance, plus the directly loaded CPU driver.
	class Instance
	{
	public:
		// Lifetime
		Instance(const char* appName, const acm::Version& appVersion, const acm::InstanceConfig& config);
		~Instance();

		// State
		bool valid() const { return m_instance != VK_NULL_HANDLE && m_error.ok(); }
		acm::Error error() const { return m_error; }
		VkInstance vulkanInstance() const { return m_instance; }
		// Whether VK_LAYER_KHRONOS_validation is enabled on this instance.
		bool validationEnabled() const { return m_validation; }

		// Messages
		// Delivers one message to this instance's handler (InstanceConfig::logHandler),
		// synchronously on the calling thread. Safe from any thread the handler is, and from
		// a Vulkan callback: it never throws. `text` and `id` may be null (treated as empty).
		void log(acm::LogSeverity severity, acm::LogCategory category, const char* text, const char* id = "") const noexcept;
		// One of the library's own warnings: a request that was degraded or ignored. Write
		// one self-contained line naming the object, what was asked and what is used instead.
		void warn(const std::string& text) const noexcept;

		// Enumeration
		const std::vector<acm::DeviceInfo>& devices() const { return m_devices; }
		VkPhysicalDevice physicalDevice(uint32_t index) const;

		// Factories
		acm::Surface createVulkanSurface(VkSurfaceKHR surface);
		acm::Surface createHeadlessSurface(acm::Extent2D extent);
		acm::Device createDevice(const acm::DeviceInfo& deviceInfo, uint32_t queueFamily, const acm::DeviceConfig& config);

	private:
		// Internals
		static constexpr uint32_t RequiredAPIVersion = VK_API_VERSION_1_3;
		static constexpr const char* ValidationLayerName = "VK_LAYER_KHRONOS_validation";
		// The CPU driver's library, as acm_stage_vulkan_runtime stages it in <exe dir>/vulkan/.
		static constexpr const char* CpuDriverLibraryName = "vk_swiftshader";
		// ACM_VALIDATION set to anything but empty or "0" turns validation and the debug
		// messenger on for every instance, whatever its config says, so a test suite or a
		// shipped binary can be validated without a rebuild.
		static bool validationForcedByEnvironment();
		static const char* resultString(VkResult result);
		static bool extensionAvailable(const std::vector<VkExtensionProperties>& extensions, const char* name);
		static bool layerAvailable(const std::vector<VkLayerProperties>& layers, const char* name);
		static VkResult createDebugMessenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* createInfo, VkDebugUtilsMessengerEXT* messenger);
		static void destroyDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger);
		static VKAPI_ATTR VkBool32 VKAPI_CALL validationCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData);
		PFN_vkGetInstanceProcAddr loadCpuDriver(std::vector<VkExtensionProperties>& extensions);
		void enumerateDevices();

		// First member, so the driver library stays loaded until everything below is gone
		// (vkDestroyInstance runs in the destructor body, before any member is destroyed).
		acm::PlatformLibrary m_cpuDriver;
		// Fixed at construction, before the first Vulkan call, so log() needs no lock.
		acm::LogHandler m_logHandler;
		VkInstance m_instance{VK_NULL_HANDLE};
		VkDebugUtilsMessengerEXT m_debugMessenger{VK_NULL_HANDLE};
		std::vector<const char*> m_layerNames;
		bool m_validation{false};
		std::vector<acm::DeviceInfo> m_devices;
		std::vector<VkPhysicalDevice> m_physicalDevices;
		acm::ResourcePool<acm::vulkan::Surface> m_surfaces;
		acm::Error m_error;
	};
} // namespace acm::vulkan
