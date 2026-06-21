#include "archimedes/acmShader.h"

#include "archimedes/acmDevice.h"

#include <vulkan/vulkan.h>

#include <cassert>

struct acm::Shader::impl
{
	acm::Device device;
	VkShaderModule shaderModule{VK_NULL_HANDLE};

	~impl()
	{
		if (shaderModule && device.valid())
		{
			VkDevice dev = device.vkDevice();
			VkShaderModule sm = shaderModule;
			device.enqueueDestroy([dev, sm]
								  { vkDestroyShaderModule(dev, sm, nullptr); });
		}
	}
};

acm::Shader::Shader(acm::Device device, const std::vector<char>& spirv)
	: m()
{
	assert(!spirv.empty() && "acm::Shader: empty SPIR-V");

	auto impl = std::make_shared<acm::Shader::impl>();
	impl->device = device;

	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = spirv.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(spirv.data());

	if (vkCreateShaderModule(impl->device.vkDevice(), &createInfo, nullptr, &impl->shaderModule) != VK_SUCCESS)
	{
		m_error = acm::Error("failed to create shader module");
		return;
	}

	m = impl;
}

VkShaderModule acm::Shader::vkShaderModule() const
{
	return m->shaderModule;
}
