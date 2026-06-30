#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace acm::vulkan
{
	class Device;

	class Shader
	{
	public:
		bool create(acm::vulkan::Device& owner, const std::vector<char>& spirv);
		acm::vulkan::Device& owner() const { return *m_owner; }
		VkShaderModule vkShaderModule() const;
		void retire(acm::vulkan::Device& owner);

	private:
		acm::vulkan::Device* m_owner{nullptr};
		VkShaderModule m_shaderModule{VK_NULL_HANDLE};
	};
} // namespace acm::vulkan
