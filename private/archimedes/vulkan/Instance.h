/*
 *  Created by LuckyNeko on 24/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmGPU.h"
#include "archimedes/acmVersion.h"
#include "archimedes/ResourcePool.h"
#include "archimedes/vulkan/Resources.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace acm::vulkan
{
	// Move-only VkInstance owner. Enumerates physical devices and owns the stable pool
	// for platform surfaces created from this instance.
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

		// Enumeration
		const std::vector<acm::GPU>& gpus() const { return m_gpus; }
		VkPhysicalDevice physicalDevice(uint32_t index) const;

		// Factories
		acm::Surface createVulkanSurface(VkSurfaceKHR surface);
		acm::Surface createHeadlessSurface(acm::Extent2D extent);
		acm::Device createDevice(const acm::GPU& gpu, uint32_t queueIndex);

	private:
		// Internals
		static constexpr uint32_t RequiredAPIVersion = VK_API_VERSION_1_3;
		static const char* resultString(VkResult result);
		static bool extensionAvailable(const std::vector<VkExtensionProperties>& extensions, const char* name);
		static bool layerAvailable(const std::vector<VkLayerProperties>& layers, const char* name);
		static VkResult createDebugMessenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* createInfo, VkDebugUtilsMessengerEXT* messenger);
		static void destroyDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger);
		static VKAPI_ATTR VkBool32 VKAPI_CALL validationCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData);
		void enumerateGPUs();

		VkInstance m_instance{VK_NULL_HANDLE};
		VkDebugUtilsMessengerEXT m_debugMessenger{VK_NULL_HANDLE};
		std::vector<const char*> m_layerNames;
		std::vector<acm::GPU> m_gpus;
		std::vector<VkPhysicalDevice> m_physicalDevices;
		acm::ResourcePool<acm::vulkan::Surface> m_surfaces;
		acm::Error m_error;
	};
} // namespace acm::vulkan
