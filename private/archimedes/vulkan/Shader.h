#pragma once

#include "archimedes/HandleMap.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace acm::vulkan
{
	class Device;

	class Shader : public acm::ResourceSlot<acm::vulkan::Shader, acm::vulkan::Device>
	{
	public:
		bool create(acm::vulkan::Device& owner, const std::vector<char>& spirv);
		VkShaderModule vkShaderModule(const acm::Handle& handle) const;
		void retire(acm::vulkan::Device& owner);

	private:
		VkShaderModule m_shaderModule{VK_NULL_HANDLE};
	};
} // namespace acm::vulkan
