#include "archimedes/vulkan/Pipeline.h"

#include "archimedes/acmPipeline.h"
#include "archimedes/vulkan/Convert.h"
#include "archimedes/vulkan/DescriptorSetLayout.h"
#include "archimedes/vulkan/Device.h"
#include "archimedes/vulkan/RenderTarget.h"
#include "archimedes/vulkan/Shader.h"

#include <algorithm>
#include <utility>
#include <vector>

bool acm::vulkan::Pipeline::create(acm::vulkan::Device& owner, const acm::PipelineConfig& config)
{
	if (!config.vertex.valid() || !config.fragment.valid() || !config.target.valid())
		return false;
	if (&config.vertex.native()->owner() != &owner || &config.fragment.native()->owner() != &owner || &config.target.native()->owner() != &owner)
		return false;
	if (config.descriptorLayout.valid() && &config.descriptorLayout.native()->owner() != &owner)
		return false;
	if (config.depthTest && !config.target.hasDepth())
		return false;
	m_owner = &owner;

	VkPipelineShaderStageCreateInfo vertStage = {};
	vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertStage.module = config.vertex.native()->vkShaderModule();
	vertStage.pName = "main";
	VkPipelineShaderStageCreateInfo fragStage = {};
	fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragStage.module = config.fragment.native()->vkShaderModule();
	fragStage.pName = "main";
	if (!vertStage.module || !fragStage.module)
		return false;
	VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

	VkVertexInputBindingDescription binding = {};
	binding.binding = 0;
	binding.stride = config.vertexLayout.stride;
	binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	std::vector<VkVertexInputAttributeDescription> attributes;
	attributes.reserve(config.vertexLayout.attributes.size());
	for (const auto& attribute : config.vertexLayout.attributes)
		attributes.push_back({attribute.location, 0, acm::vulkan::toVk(attribute.format), attribute.offset});
	VkPipelineVertexInputStateCreateInfo vertexInput = {};
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	if (config.vertexLayout.stride > 0 && !attributes.empty())
	{
		vertexInput.vertexBindingDescriptionCount = 1;
		vertexInput.pVertexBindingDescriptions = &binding;
		vertexInput.vertexAttributeDescriptionCount = uint32_t(attributes.size());
		vertexInput.pVertexAttributeDescriptions = attributes.data();
	}

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = acm::vulkan::toVk(config.topology);
	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;
	VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;

	acm::PolygonMode polygonMode = config.polygonMode;
	if (polygonMode == acm::PolygonMode::Line && !owner.enabledFeatures().fillModeNonSolid)
		polygonMode = acm::PolygonMode::Fill;
	float lineWidth = config.lineWidth;
	if (lineWidth != 1.0f && !owner.enabledFeatures().wideLines)
		lineWidth = 1.0f;
	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.polygonMode = acm::vulkan::toVk(polygonMode);
	rasterizer.lineWidth = lineWidth;
	rasterizer.cullMode = acm::vulkan::toVk(config.cullMode);
	rasterizer.frontFace = acm::vulkan::toVk(config.frontFace);

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = config.target.native()->sampleCount();
	if (config.minSampleShading > 0.0f && multisampling.rasterizationSamples != VK_SAMPLE_COUNT_1_BIT && owner.enabledFeatures().sampleRateShading)
	{
		multisampling.sampleShadingEnable = VK_TRUE;
		multisampling.minSampleShading = std::min(config.minSampleShading, 1.0f);
	}

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	if (config.blend == acm::BlendMode::AlphaBlend)
	{
		colorBlendAttachment.blendEnable = VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	}
	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	const VkFormat colorFormat = config.target.native()->colorFormat();
	if (colorFormat == VK_FORMAT_UNDEFINED)
		return false;
	VkPipelineRenderingCreateInfo renderingInfo = {};
	renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachmentFormats = &colorFormat;
	renderingInfo.depthAttachmentFormat = config.target.native()->depthFormat();

	const VkDescriptorSetLayout setLayout = config.descriptorLayout.valid() ? config.descriptorLayout.native()->vkLayout() : VK_NULL_HANDLE;
	VkPipelineLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	if (setLayout)
	{
		layoutInfo.setLayoutCount = 1;
		layoutInfo.pSetLayouts = &setLayout;
	}
	if (vkCreatePipelineLayout(owner.vkDevice(), &layoutInfo, nullptr, &m_layout) != VK_SUCCESS)
		return false;

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext = &renderingInfo;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = stages;
	pipelineInfo.pVertexInputState = &vertexInput;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDepthStencilState = config.depthTest ? &depthStencil : nullptr;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = m_layout;
	if (vkCreateGraphicsPipelines(owner.vkDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS)
		return false;

	m_descriptorLayout = config.descriptorLayout;
	return true;
}

VkPipeline acm::vulkan::Pipeline::vkPipeline() const
{
	return m_pipeline;
}

VkPipelineLayout acm::vulkan::Pipeline::vkLayout() const
{
	return m_layout;
}

void acm::vulkan::Pipeline::retire(acm::vulkan::Device& owner)
{
	const VkDevice device = owner.vkDevice();
	const VkPipeline pipeline = std::exchange(m_pipeline, VK_NULL_HANDLE);
	const VkPipelineLayout layout = std::exchange(m_layout, VK_NULL_HANDLE);
	m_owner = nullptr;
	if (pipeline)
		owner.enqueueDestroy([device, pipeline]
							 { vkDestroyPipeline(device, pipeline, nullptr); });
	if (layout)
		owner.enqueueDestroy([device, layout]
							 { vkDestroyPipelineLayout(device, layout, nullptr); });
	m_descriptorLayout.reset();
}
