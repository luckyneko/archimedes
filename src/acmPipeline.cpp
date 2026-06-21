#include "archimedes/acmPipeline.h"

#include "archimedes/acmDescriptorSetLayout.h"
#include "archimedes/acmDevice.h"
#include "archimedes/acmShader.h"
#include "archimedes/acmVkConvert.h"

#include <vulkan/vulkan.h>

#include <cassert>
#include <vector>

struct acm::Pipeline::impl
{
	acm::Device device;
	VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
	VkPipeline pipeline{VK_NULL_HANDLE};

	~impl()
	{
		if (!device.valid())
			return;

		// Defer onto the device's frame-fenced queue; enqueue order is run order,
		// so pipeline before its layout (the safe ordering).
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

acm::Pipeline::Pipeline(acm::Device device, const acm::PipelineConfig& config)
	: m()
{
	assert(config.vertex.valid() && config.fragment.valid() && "acm::Pipeline: invalid shader");

	auto impl = std::make_shared<acm::Pipeline::impl>();
	impl->device = device;

	VkPipelineShaderStageCreateInfo vertStage = {};
	vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertStage.module = config.vertex.vkShaderModule();
	vertStage.pName = "main";

	VkPipelineShaderStageCreateInfo fragStage = {};
	fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragStage.module = config.fragment.vkShaderModule();
	fragStage.pName = "main";

	VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

	// Vertex input from the config's layout. An empty layout (stride 0 / no
	// attributes) leaves it empty — geometry then comes from the shader.
	VkVertexInputBindingDescription binding = {};
	binding.binding = 0;
	binding.stride = config.vertexLayout.stride;
	binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	std::vector<VkVertexInputAttributeDescription> attributes;
	attributes.reserve(config.vertexLayout.attributes.size());
	for (const auto& a : config.vertexLayout.attributes)
	{
		VkVertexInputAttributeDescription desc = {};
		desc.location = a.location;
		desc.binding = 0;
		desc.format = acm::toVk(a.format);
		desc.offset = a.offset;
		attributes.push_back(desc);
	}

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
	inputAssembly.topology = acm::toVk(config.topology);

	// Viewport + scissor are dynamic: counts are fixed here, the actual rects are
	// set per-frame via vkCmdSetViewport/Scissor (see CommandBuffer). This keeps the
	// pipeline independent of the swapchain size, so resize needs no pipeline rebuild.
	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;

	// Wireframe / wide lines need device features; fall back silently when missing.
	const acm::GPUFeatures& features = device.enabledFeatures();
	acm::PolygonMode polygonMode = config.polygonMode;
	if (polygonMode == acm::PolygonMode::Line && !features.fillModeNonSolid)
		polygonMode = acm::PolygonMode::Fill;
	float lineWidth = config.lineWidth;
	if (lineWidth != 1.0f && !features.wideLines)
		lineWidth = 1.0f;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.polygonMode = acm::toVk(polygonMode);
	rasterizer.lineWidth = lineWidth;
	rasterizer.cullMode = acm::toVk(config.cullMode);
	rasterizer.frontFace = acm::toVk(config.frontFace);

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = acm::toVkSampleCount(config.samples, device.getGPU().device);

	// Per-sample shading: only meaningful when multisampled, and needs the feature.
	if (config.minSampleShading > 0.0f && multisampling.rasterizationSamples != VK_SAMPLE_COUNT_1_BIT)
	{
		if (features.sampleRateShading)
		{
			multisampling.sampleShadingEnable = VK_TRUE;
			multisampling.minSampleShading = config.minSampleShading > 1.0f ? 1.0f : config.minSampleShading;
		}
	}

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	if (config.blend == acm::BlendMode::AlphaBlend)
	{
		// Standard src-alpha "over": out = src.rgb*src.a + dst.rgb*(1-src.a).
		colorBlendAttachment.blendEnable = VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	}
	else
	{
		colorBlendAttachment.blendEnable = VK_FALSE;
	}

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	// Depth: standard test + write with LESS (nearer fragments win). Only attached to
	// the pipeline when requested — must line up with a depth-bearing render pass.
	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	VkDescriptorSetLayout setLayout = config.descriptorLayout.valid() ? config.descriptorLayout.vkDescriptorSetLayout() : VK_NULL_HANDLE;
	VkPipelineLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	if (setLayout != VK_NULL_HANDLE)
	{
		layoutInfo.setLayoutCount = 1;
		layoutInfo.pSetLayouts = &setLayout;
	}

	if (vkCreatePipelineLayout(impl->device.vkDevice(), &layoutInfo, nullptr, &impl->pipelineLayout) != VK_SUCCESS)
	{
		m_error = acm::Error("failed to create pipeline layout");
		return;
	}

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
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
	pipelineInfo.layout = impl->pipelineLayout;
	pipelineInfo.renderPass = config.renderPass;
	pipelineInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(impl->device.vkDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &impl->pipeline) != VK_SUCCESS)
	{
		m_error = acm::Error("failed to create graphics pipeline");
		return;
	}

	m = impl;
}

VkPipeline acm::Pipeline::vkPipeline() const
{
	return m->pipeline;
}

VkPipelineLayout acm::Pipeline::vkPipelineLayout() const
{
	return m->pipelineLayout;
}
