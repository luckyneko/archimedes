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
	class Instance
	{
	public:
		Instance(const char* appName, const acm::Version& appVersion, const acm::InstanceConfig& config);
		~Instance();

		bool valid() const { return m_instance != VK_NULL_HANDLE && m_error.ok(); }
		acm::Error error() const { return m_error; }
		VkInstance nativeInstance() const { return m_instance; }
		const std::vector<acm::GPU>& gpus() const { return m_gpus; }
		VkPhysicalDevice physicalDevice(uint32_t index) const;

		acm::Surface createSurface(VkSurfaceKHR surface);
		acm::Device createDevice(const acm::GPU& gpu, uint32_t queueIdx);

	private:
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
