#include "archimedes/vulkan/ComputePipeline.h"

#include "archimedes/vulkan/DescriptorSetLayout.h"
#include "archimedes/vulkan/Device.h"
#include "archimedes/vulkan/Shader.h"

#include <utility>

bool acm::vulkan::ComputePipeline::create(acm::vulkan::Device& owner, acm::vulkan::Shader& compute, const acm::Handle& computeHandle, acm::vulkan::DescriptorSetLayout* layout, const acm::Handle& layoutHandle)
{
	if (&compute.owner() != &owner || (layout && &layout->owner() != &owner))
		return false;
	const VkShaderModule shaderModule = compute.vkShaderModule(computeHandle);
	const VkDescriptorSetLayout setLayout = layout ? layout->vkLayout(layoutHandle) : VK_NULL_HANDLE;
	if (!shaderModule || (layout && !setLayout))
		return false;

	VkPipelineLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	if (setLayout)
	{
		layoutInfo.setLayoutCount = 1;
		layoutInfo.pSetLayouts = &setLayout;
	}
	if (vkCreatePipelineLayout(owner.vkDevice(), &layoutInfo, nullptr, &m_layout) != VK_SUCCESS)
		return false;

	VkPipelineShaderStageCreateInfo stage = {};
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = shaderModule;
	stage.pName = "main";
	VkComputePipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.stage = stage;
	pipelineInfo.layout = m_layout;
	if (vkCreateComputePipelines(owner.vkDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS)
		return false;

	if (layout)
	{
		m_descriptorLayoutResource = layout;
		m_descriptorLayout = layoutHandle;
		if (!layout->retain(layoutHandle))
			return false;
	}
	return true;
}

VkPipeline acm::vulkan::ComputePipeline::vkPipeline(const acm::Handle& handle) const
{
	return accessible(handle) ? m_pipeline : VK_NULL_HANDLE;
}

VkPipelineLayout acm::vulkan::ComputePipeline::vkLayout(const acm::Handle& handle) const
{
	return accessible(handle) ? m_layout : VK_NULL_HANDLE;
}

void acm::vulkan::ComputePipeline::retire(acm::vulkan::Device& owner)
{
	const VkDevice device = owner.vkDevice();
	const VkPipeline pipeline = std::exchange(m_pipeline, VK_NULL_HANDLE);
	const VkPipelineLayout layout = std::exchange(m_layout, VK_NULL_HANDLE);
	if (pipeline)
		owner.enqueueDestroy([device, pipeline]
							 { vkDestroyPipeline(device, pipeline, nullptr); });
	if (layout)
		owner.enqueueDestroy([device, layout]
							 { vkDestroyPipelineLayout(device, layout, nullptr); });
	if (m_descriptorLayoutResource)
	{
		auto* descriptorLayout = std::exchange(m_descriptorLayoutResource, nullptr);
		const acm::Handle descriptorLayoutHandle = std::exchange(m_descriptorLayout, {});
		descriptorLayout->release(descriptorLayoutHandle);
	}
}
