#include "MainDelegate.h"

#include <archimedes/archimedes.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace
{
	constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;

	std::vector<char> readFile(const std::string& path)
	{
		std::ifstream file(path, std::ios::ate | std::ios::binary);
		std::vector<char> buffer;
		if(!file.is_open())
		{
			spdlog::error("failed to open shader: {0}", path);
			return buffer;
		}
		buffer.resize(size_t(file.tellg()));
		file.seekg(0);
		file.read(buffer.data(), std::streamsize(buffer.size()));
		return buffer;
	}

	VkShaderModule createShaderModule(acm::Device device, const std::vector<char>& code)
	{
		if(code.empty())
			return VK_NULL_HANDLE;

		VkShaderModuleCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		VkShaderModule shaderModule = VK_NULL_HANDLE;
		if(vkCreateShaderModule(device.vkDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
			spdlog::error("failed to create shader module");
		return shaderModule;
	}
}

SwapChainSettings MainDelegate::onSelectSwapChainSettings(const std::vector<acm::GPU>& gpus, const std::vector<acm::GPUSurfaceSupport>& surfaceSupport)
{
	SwapChainSettings result;
	for(const auto& gpu : gpus)
	{
		// Must have surface support at all
		auto gpuSupportIt = std::find_if(surfaceSupport.begin(), surfaceSupport.end(),
			[targetIdx = gpu.index](const acm::GPUSurfaceSupport& i) { return i.gpuIndex == targetIdx; });
		if(gpuSupportIt == surfaceSupport.end())
		{
			spdlog::debug("no surface support for: {0}", gpu.properties.deviceName);
			continue;
		}
		const acm::GPUSurfaceSupport& gpuSupport = *gpuSupportIt;

		// Must expose at least one format + present mode
		if(gpuSupport.supportedFormats.empty() || gpuSupport.supportedPresentModes.empty())
		{
			spdlog::debug("no surface format/mode for: {0}", gpu.properties.deviceName);
			continue;
		}

		// First queue family that supports graphics + present wins
		for(const auto& queueFamily : gpu.queueFamilies)
		{
			bool supportsPresent = gpuSupport.queueFamilySupportsPresent[queueFamily.index];
			if(queueFamily.supportsGraphics && supportsPresent)
			{
				result.selectedGPUIdx = gpu.index;
				result.selectedQueueFamilyIdx = queueFamily.index;
				result.selectedFormat = gpuSupport.supportedFormats[0];
				result.selectedPresentMode = gpuSupport.supportedPresentModes[0];
				return result;
			}
		}
		spdlog::debug("no graphics+present queue for: {0}", gpu.properties.deviceName);
	}

	return result;
}

bool MainDelegate::createPipeline(acm::Device device, acm::SwapChain swapChain)
{
	auto vertCode = readFile(std::string(TESTBED_SHADER_DIR) + "/triangle.vert.spv");
	auto fragCode = readFile(std::string(TESTBED_SHADER_DIR) + "/triangle.frag.spv");
	VkShaderModule vertModule = createShaderModule(device, vertCode);
	VkShaderModule fragModule = createShaderModule(device, fragCode);
	if(vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE)
		return false;

	VkPipelineShaderStageCreateInfo vertStage = {};
	vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertStage.module = vertModule;
	vertStage.pName = "main";

	VkPipelineShaderStageCreateInfo fragStage = {};
	fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragStage.module = fragModule;
	fragStage.pName = "main";

	VkPipelineShaderStageCreateInfo stages[] = { vertStage, fragStage };

	// Geometry is hardcoded in the vertex shader — no vertex input bindings.
	VkPipelineVertexInputStateCreateInfo vertexInput = {};
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkViewport viewport = {};
	viewport.width = float(swapChain.getExtents().width);
	viewport.height = float(swapChain.getExtents().height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.extent = swapChain.getExtents();

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_NONE; // smoke test: never cull the triangle
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	VkPipelineLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	bool ok = (vkCreatePipelineLayout(device.vkDevice(), &layoutInfo, nullptr, &m_pipelineLayout) == VK_SUCCESS);
	if(!ok)
		spdlog::error("failed to create pipeline layout");

	if(ok)
	{
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
		pipelineInfo.layout = m_pipelineLayout;
		pipelineInfo.renderPass = swapChain.getRenderTarget(0).vkRenderPass(); // all targets share a compatible pass
		pipelineInfo.subpass = 0;

		ok = (vkCreateGraphicsPipelines(device.vkDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) == VK_SUCCESS);
		if(!ok)
			spdlog::error("failed to create graphics pipeline");
	}

	vkDestroyShaderModule(device.vkDevice(), fragModule, nullptr);
	vkDestroyShaderModule(device.vkDevice(), vertModule, nullptr);
	return ok;
}

bool MainDelegate::createCommandBuffers(acm::Device device, acm::SwapChain swapChain)
{
	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = device.getQueueIdx();
	if(vkCreateCommandPool(device.vkDevice(), &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS)
	{
		spdlog::error("failed to create command pool");
		return false;
	}

	m_commandBuffers.resize(swapChain.getRenderTargetCount());

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = uint32_t(m_commandBuffers.size());
	if(vkAllocateCommandBuffers(device.vkDevice(), &allocInfo, m_commandBuffers.data()) != VK_SUCCESS)
	{
		spdlog::error("failed to allocate command buffers");
		return false;
	}

	for(size_t i = 0; i < m_commandBuffers.size(); ++i)
	{
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		if(vkBeginCommandBuffer(m_commandBuffers[i], &beginInfo) != VK_SUCCESS)
		{
			spdlog::error("failed to begin command buffer");
			return false;
		}

		auto renderTarget = swapChain.getRenderTarget(i);
		VkClearValue clearColor = {{{ 0.0f, 0.0f, 0.0f, 1.0f }}};
		VkRenderPassBeginInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderTarget.vkRenderPass();
		renderPassInfo.framebuffer = renderTarget.vkFramebuffer();
		renderPassInfo.renderArea.extent = swapChain.getExtents();
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(m_commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
		vkCmdDraw(m_commandBuffers[i], 3, 1, 0, 0);
		vkCmdEndRenderPass(m_commandBuffers[i]);

		if(vkEndCommandBuffer(m_commandBuffers[i]) != VK_SUCCESS)
		{
			spdlog::error("failed to record command buffer");
			return false;
		}
	}

	return true;
}

bool MainDelegate::createSyncObjects(acm::Device device)
{
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	m_frames.resize(MAX_FRAMES_IN_FLIGHT);
	for(auto& frame : m_frames)
	{
		if(vkCreateSemaphore(device.vkDevice(), &semaphoreInfo, nullptr, &frame.imageAvailable) != VK_SUCCESS ||
			vkCreateSemaphore(device.vkDevice(), &semaphoreInfo, nullptr, &frame.renderFinished) != VK_SUCCESS ||
			vkCreateFence(device.vkDevice(), &fenceInfo, nullptr, &frame.inFlight) != VK_SUCCESS)
		{
			spdlog::error("failed to create sync objects");
			return false;
		}
	}
	return true;
}

void MainDelegate::onInit(acm::Device device, acm::SwapChain swapChain)
{
	bool ok = createPipeline(device, swapChain);
	spdlog::info("CreatePipeline: {0}", ok ? "ok" : "FAIL");
	ok = ok && createCommandBuffers(device, swapChain);
	spdlog::info("CreateCommandBuffers: {0}", ok ? "ok" : "FAIL");
	ok = ok && createSyncObjects(device);
	spdlog::info("CreateSyncObjects: {0}", ok ? "ok" : "FAIL");
	m_ready = ok;
}

void MainDelegate::onShutdown(acm::Device device, acm::SwapChain)
{
	if(!device.valid())
		return;

	vkDeviceWaitIdle(device.vkDevice());

	for(auto& frame : m_frames)
	{
		if(frame.renderFinished) vkDestroySemaphore(device.vkDevice(), frame.renderFinished, nullptr);
		if(frame.imageAvailable) vkDestroySemaphore(device.vkDevice(), frame.imageAvailable, nullptr);
		if(frame.inFlight) vkDestroyFence(device.vkDevice(), frame.inFlight, nullptr);
	}
	m_frames.clear();

	if(!m_commandBuffers.empty())
		vkFreeCommandBuffers(device.vkDevice(), m_commandPool, uint32_t(m_commandBuffers.size()), m_commandBuffers.data());
	if(m_commandPool) vkDestroyCommandPool(device.vkDevice(), m_commandPool, nullptr);
	if(m_pipeline) vkDestroyPipeline(device.vkDevice(), m_pipeline, nullptr);
	if(m_pipelineLayout) vkDestroyPipelineLayout(device.vkDevice(), m_pipelineLayout, nullptr);
}

void MainDelegate::onUpdate()
{
}

void MainDelegate::onRender(acm::Device device, acm::SwapChain swapChain)
{
	if(!m_ready)
		return;

	auto& frame = m_frames[m_currentFrame];
	vkWaitForFences(device.vkDevice(), 1, &frame.inFlight, VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(device.vkDevice(), 1, &frame.inFlight);

	uint32_t imageIndex = 0;
	vkAcquireNextImageKHR(device.vkDevice(), swapChain.vkSwapChain(), std::numeric_limits<uint64_t>::max(), frame.imageAvailable, VK_NULL_HANDLE, &imageIndex);

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &frame.imageAvailable;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_commandBuffers[imageIndex];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &frame.renderFinished;

	if(vkQueueSubmit(device.vkQueue(), 1, &submitInfo, frame.inFlight) != VK_SUCCESS)
		spdlog::error("failed to submit draw command buffer");

	VkSwapchainKHR swapChains[] = { swapChain.vkSwapChain() };
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &frame.renderFinished;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;
	vkQueuePresentKHR(device.vkQueue(), &presentInfo);

	m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}
