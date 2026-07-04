/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

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

namespace acm::vulkan
{

	// -----------------------------------------------------------------------------
	// Lifetime
	// -----------------------------------------------------------------------------

	Pipeline::Pipeline(Device& owner, const acm::PipelineShaders& shaders, const acm::RenderTarget& target, const acm::PipelineConfig& config)
	{
		if (shaders.geometry.valid())
		{
			m_error = acm::Error("failed to create pipeline: geometry shaders are not supported yet");
			return;
		}
		if (!shaders.vertex.valid() || !shaders.fragment.valid() || !target.valid())
		{
			m_error = acm::Error("failed to create pipeline from invalid shader or render target");
			return;
		}
		if (&shaders.vertex.backend()->owner() != &owner || &shaders.fragment.backend()->owner() != &owner || &target.backend()->owner() != &owner)
		{
			m_error = acm::Error("failed to create pipeline from resources owned by another device");
			return;
		}
		if (config.descriptorLayout.valid() && &config.descriptorLayout.backend()->owner() != &owner)
		{
			m_error = acm::Error("failed to create pipeline from descriptor layout owned by another device");
			return;
		}
		const bool usesDepth = config.depth.test || config.depth.write;
		if (config.depth.write && !config.depth.test)
		{
			m_error = acm::Error("failed to create depth-write pipeline without depth test");
			return;
		}
		if (usesDepth && !target.hasDepth())
		{
			m_error = acm::Error("failed to create depth pipeline for target without depth");
			return;
		}
		m_owner = &owner;

		VkPipelineShaderStageCreateInfo vertStage = {};
		vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertStage.module = shaders.vertex.backend()->vkShaderModule();
		vertStage.pName = "main";
		VkPipelineShaderStageCreateInfo fragStage = {};
		fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragStage.module = shaders.fragment.backend()->vkShaderModule();
		fragStage.pName = "main";
		if (!vertStage.module || !fragStage.module)
		{
			m_error = acm::Error("failed to create pipeline from invalid shader module");
			return;
		}
		VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

		VkVertexInputBindingDescription binding = {};
		binding.binding = 0;
		binding.stride = config.vertexLayout.stride;
		binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		std::vector<VkVertexInputAttributeDescription> attributes;
		attributes.reserve(config.vertexLayout.attributes.size());
		for (const auto& attribute : config.vertexLayout.attributes)
			attributes.push_back({attribute.location, 0, toVk(attribute.format), attribute.offset});
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
		inputAssembly.topology = toVk(config.topology);
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
		rasterizer.polygonMode = toVk(polygonMode);
		rasterizer.lineWidth = lineWidth;
		rasterizer.cullMode = toVk(config.cullMode);
		rasterizer.frontFace = toVk(config.frontFace);

		VkPipelineMultisampleStateCreateInfo multisampling = {};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.rasterizationSamples = target.backend()->sampleCount();
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
		depthStencil.depthTestEnable = config.depth.test ? VK_TRUE : VK_FALSE;
		depthStencil.depthWriteEnable = config.depth.write ? VK_TRUE : VK_FALSE;
		depthStencil.depthCompareOp = toVk(config.depth.compare);
		const VkFormat colorFormat = target.backend()->colorFormat();
		if (colorFormat == VK_FORMAT_UNDEFINED)
		{
			m_error = acm::Error("failed to create pipeline from invalid render target format");
			return;
		}
		m_colorFormat = colorFormat;
		m_depthFormat = target.backend()->depthFormat();
		m_sampleCount = target.backend()->sampleCount();
		VkPipelineRenderingCreateInfo renderingInfo = {};
		renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachmentFormats = &colorFormat;
		renderingInfo.depthAttachmentFormat = m_depthFormat;

		const VkDescriptorSetLayout setLayout = config.descriptorLayout.valid() ? config.descriptorLayout.backend()->vkLayout() : VK_NULL_HANDLE;
		VkPipelineLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		if (setLayout)
		{
			layoutInfo.setLayoutCount = 1;
			layoutInfo.pSetLayouts = &setLayout;
		}
		if (vkCreatePipelineLayout(owner.vkDevice(), &layoutInfo, nullptr, &m_layout) != VK_SUCCESS)
		{
			m_error = acm::Error("failed to create pipeline layout");
			return;
		}

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
		pipelineInfo.pDepthStencilState = usesDepth ? &depthStencil : nullptr;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = m_layout;
		if (vkCreateGraphicsPipelines(owner.vkDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS)
		{
			m_error = acm::Error("failed to create graphics pipeline");
			return;
		}

		m_descriptorLayout = config.descriptorLayout;
	}

	Pipeline::~Pipeline()
	{
		release();
	}

	Pipeline::Pipeline(Pipeline&& other) noexcept
	{
		*this = std::move(other);
	}

	Pipeline& Pipeline::operator=(Pipeline&& other) noexcept
	{
		if (this == &other)
			return *this;
		release();
		m_owner = std::exchange(other.m_owner, nullptr);
		m_descriptorLayout = std::move(other.m_descriptorLayout);
		m_colorFormat = std::exchange(other.m_colorFormat, VK_FORMAT_UNDEFINED);
		m_depthFormat = std::exchange(other.m_depthFormat, VK_FORMAT_UNDEFINED);
		m_sampleCount = std::exchange(other.m_sampleCount, VK_SAMPLE_COUNT_1_BIT);
		m_layout = std::exchange(other.m_layout, VK_NULL_HANDLE);
		m_pipeline = std::exchange(other.m_pipeline, VK_NULL_HANDLE);
		m_error = std::move(other.m_error);
		return *this;
	}

	// -----------------------------------------------------------------------------
	// State
	// -----------------------------------------------------------------------------

	VkPipeline Pipeline::vkPipeline() const
	{
		return m_pipeline;
	}

	VkPipelineLayout Pipeline::vkLayout() const
	{
		return m_layout;
	}

	bool Pipeline::compatibleWith(const RenderTarget& target) const
	{
		return valid() && target.valid() && m_colorFormat == target.colorFormat() && m_depthFormat == target.depthFormat() && m_sampleCount == target.sampleCount();
	}

	// -----------------------------------------------------------------------------
	// Internals
	// -----------------------------------------------------------------------------

	void Pipeline::release()
	{
		Device* owner = std::exchange(m_owner, nullptr);
		const VkPipeline pipeline = std::exchange(m_pipeline, VK_NULL_HANDLE);
		const VkPipelineLayout layout = std::exchange(m_layout, VK_NULL_HANDLE);
		if (owner && pipeline)
			vkDestroyPipeline(owner->vkDevice(), pipeline, nullptr);
		if (owner && layout)
			vkDestroyPipelineLayout(owner->vkDevice(), layout, nullptr);
		m_descriptorLayout.reset();
		m_colorFormat = VK_FORMAT_UNDEFINED;
		m_depthFormat = VK_FORMAT_UNDEFINED;
		m_sampleCount = VK_SAMPLE_COUNT_1_BIT;
	}

} // namespace acm::vulkan
