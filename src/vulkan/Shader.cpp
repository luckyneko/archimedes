#include "archimedes/vulkan/Shader.h"

#include "archimedes/vulkan/Device.h"

bool acm::vulkan::Shader::create(acm::vulkan::Device& owner, const std::vector<char>& spirv)
{
	if (spirv.empty())
		return false;
	m_owner = &owner;
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = spirv.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(spirv.data());
	return vkCreateShaderModule(owner.vkDevice(), &createInfo, nullptr, &m_shaderModule) == VK_SUCCESS;
}

VkShaderModule acm::vulkan::Shader::vkShaderModule() const
{
	return m_shaderModule;
}

void acm::vulkan::Shader::retire(acm::vulkan::Device& owner)
{
	const VkShaderModule retiredModule = std::exchange(m_shaderModule, VK_NULL_HANDLE);
	m_owner = nullptr;
	if (!retiredModule)
		return;
	const VkDevice device = owner.vkDevice();
	owner.enqueueDestroy([device, retiredModule]
						 { vkDestroyShaderModule(device, retiredModule, nullptr); });
}
