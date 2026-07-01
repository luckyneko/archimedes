#include "archimedes/vulkan/ComputePipeline.h"

#include "archimedes/vulkan/DescriptorSetLayout.h"
#include "archimedes/vulkan/Device.h"
#include "archimedes/vulkan/Shader.h"

#include <utility>

acm::vulkan::ComputePipeline::ComputePipeline(acm::vulkan::Device& owner, const acm::Shader& compute, const acm::DescriptorSetLayout& layout)
{
	if (!compute.valid() || !compute.native() || &compute.native()->owner() != &owner)
	{
		m_error = acm::Error("failed to create compute pipeline from invalid shader");
		return;
	}
	if (layout.valid() && (!layout.native() || &layout.native()->owner() != &owner))
	{
		m_error = acm::Error("failed to create compute pipeline from descriptor layout owned by another device");
		return;
	}
	m_owner = &owner;
	const VkShaderModule shaderModule = compute.native()->vkShaderModule();
	const VkDescriptorSetLayout setLayout = layout.valid() ? layout.native()->vkLayout() : VK_NULL_HANDLE;
	if (!shaderModule || (layout.valid() && !setLayout))
	{
		m_error = acm::Error("failed to create compute pipeline from invalid shader or descriptor layout");
		return;
	}

	VkPipelineLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	if (setLayout)
	{
		layoutInfo.setLayoutCount = 1;
		layoutInfo.pSetLayouts = &setLayout;
	}
	if (vkCreatePipelineLayout(owner.vkDevice(), &layoutInfo, nullptr, &m_layout) != VK_SUCCESS)
	{
		m_error = acm::Error("failed to create compute pipeline layout");
		return;
	}

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
	{
		m_error = acm::Error("failed to create compute pipeline");
		return;
	}

	m_descriptorLayout = layout;
}

acm::vulkan::ComputePipeline::~ComputePipeline()
{
	release();
}

acm::vulkan::ComputePipeline::ComputePipeline(ComputePipeline&& other) noexcept
{
	*this = std::move(other);
}

acm::vulkan::ComputePipeline& acm::vulkan::ComputePipeline::operator=(ComputePipeline&& other) noexcept
{
	if (this == &other)
		return *this;
	release();
	m_owner = std::exchange(other.m_owner, nullptr);
	m_descriptorLayout = std::move(other.m_descriptorLayout);
	m_layout = std::exchange(other.m_layout, VK_NULL_HANDLE);
	m_pipeline = std::exchange(other.m_pipeline, VK_NULL_HANDLE);
	m_error = std::move(other.m_error);
	return *this;
}

VkPipeline acm::vulkan::ComputePipeline::vkPipeline() const
{
	return m_pipeline;
}

VkPipelineLayout acm::vulkan::ComputePipeline::vkLayout() const
{
	return m_layout;
}

void acm::vulkan::ComputePipeline::release()
{
	acm::vulkan::Device* owner = std::exchange(m_owner, nullptr);
	const VkPipeline pipeline = std::exchange(m_pipeline, VK_NULL_HANDLE);
	const VkPipelineLayout layout = std::exchange(m_layout, VK_NULL_HANDLE);
	if (owner && pipeline)
		vkDestroyPipeline(owner->vkDevice(), pipeline, nullptr);
	if (owner && layout)
		vkDestroyPipelineLayout(owner->vkDevice(), layout, nullptr);
	m_descriptorLayout.reset();
}
