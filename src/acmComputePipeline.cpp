#include "archimedes/acmComputePipeline.h"
#include "archimedes/acmDescriptorSetLayout.h"
#include "archimedes/acmDevice.h"
#include "archimedes/acmShader.h"
#include <cassert>
#include <vulkan/vulkan.h>

struct acm::ComputePipeline::impl
{
	acm::Device device;
	VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
	VkPipeline pipeline{VK_NULL_HANDLE};

	~impl()
	{
		if (!device.valid())
			return;

		// Defer onto the device's frame-fenced queue; enqueue order is run order, so the
		// pipeline goes before its layout (the safe ordering).
		VkDevice dev = device.vkDevice();
		if (pipeline)
		{
			VkPipeline p = pipeline;
			device.enqueueDestroy([dev, p]
								  { vkDestroyPipeline(dev, p, nullptr); });
		}
		if (pipelineLayout)
		{
			VkPipelineLayout pl = pipelineLayout;
			device.enqueueDestroy([dev, pl]
								  { vkDestroyPipelineLayout(dev, pl, nullptr); });
		}
	}
};

acm::ComputePipeline::ComputePipeline(acm::Device device, acm::Shader compute, acm::DescriptorSetLayout layout)
	: m()
{
	assert(compute.valid() && "acm::ComputePipeline: invalid shader");

	auto impl = std::make_shared<acm::ComputePipeline::impl>();
	impl->device = device;

	VkDescriptorSetLayout setLayout = layout.valid() ? layout.vkDescriptorSetLayout() : VK_NULL_HANDLE;
	VkPipelineLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	if (setLayout != VK_NULL_HANDLE)
	{
		layoutInfo.setLayoutCount = 1;
		layoutInfo.pSetLayouts = &setLayout;
	}
	if (vkCreatePipelineLayout(impl->device.vkDevice(), &layoutInfo, nullptr, &impl->pipelineLayout) != VK_SUCCESS)
	{
		m_error = acm::Error("failed to create compute pipeline layout");
		return;
	}

	VkPipelineShaderStageCreateInfo stage = {};
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = compute.vkShaderModule();
	stage.pName = "main";

	VkComputePipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.stage = stage;
	pipelineInfo.layout = impl->pipelineLayout;

	if (vkCreateComputePipelines(impl->device.vkDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &impl->pipeline) != VK_SUCCESS)
	{
		m_error = acm::Error("failed to create compute pipeline");
		return;
	}

	m = impl;
}

VkPipeline acm::ComputePipeline::vkPipeline() const
{
	return m->pipeline;
}

VkPipelineLayout acm::ComputePipeline::vkPipelineLayout() const
{
	return m->pipelineLayout;
}
