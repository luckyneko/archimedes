#include "archimedes/vulkan/Shader.h"

#include "archimedes/vulkan/Device.h"

#include <utility>

acm::vulkan::Shader::Shader(acm::vulkan::Device& owner, const std::vector<char>& spirv)
{
	if (spirv.empty())
	{
		m_error = acm::Error("failed to create shader from empty SPIR-V");
		return;
	}
	m_owner = &owner;
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = spirv.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(spirv.data());
	if (vkCreateShaderModule(owner.vkDevice(), &createInfo, nullptr, &m_shaderModule) != VK_SUCCESS)
		m_error = acm::Error("failed to create shader module");
}

acm::vulkan::Shader::~Shader()
{
	release();
}

acm::vulkan::Shader::Shader(Shader&& other) noexcept
{
	*this = std::move(other);
}

acm::vulkan::Shader& acm::vulkan::Shader::operator=(Shader&& other) noexcept
{
	if (this == &other)
		return *this;
	release();
	m_owner = std::exchange(other.m_owner, nullptr);
	m_shaderModule = std::exchange(other.m_shaderModule, VK_NULL_HANDLE);
	m_error = std::move(other.m_error);
	return *this;
}

VkShaderModule acm::vulkan::Shader::vkShaderModule() const
{
	return m_shaderModule;
}

void acm::vulkan::Shader::release()
{
	acm::vulkan::Device* owner = std::exchange(m_owner, nullptr);
	const VkShaderModule module = std::exchange(m_shaderModule, VK_NULL_HANDLE);
	if (!owner || !module)
		return;
	vkDestroyShaderModule(owner->vkDevice(), module, nullptr);
}
