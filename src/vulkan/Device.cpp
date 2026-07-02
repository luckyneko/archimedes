/*
 *  Created by LuckyNeko on 24/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/vulkan/Device.h"

#include "archimedes/acmBuffer.h"
#include "archimedes/acmCommandBuffer.h"
#include "archimedes/acmCommandPool.h"
#include "archimedes/acmComputePipeline.h"
#include "archimedes/acmDescriptorSet.h"
#include "archimedes/acmDescriptorSetLayout.h"
#include "archimedes/acmPipeline.h"
#include "archimedes/acmRenderer.h"
#include "archimedes/acmRenderTarget.h"
#include "archimedes/acmSampler.h"
#include "archimedes/acmShader.h"
#include "archimedes/acmSurface.h"
#include "archimedes/acmSwapChain.h"
#include "archimedes/acmTexture.h"
#include "archimedes/vulkan/Instance.h"

#include <cstring>
#include <utility>

acm::vulkan::Device::Device(acm::vulkan::Instance& instance, const acm::GPU& gpu, uint32_t queueIdx)
	: m_instance(&instance)
{
	if (!m_instance || !m_instance->valid() || gpu.index >= m_instance->gpus().size())
	{
		m_error = acm::Error("failed to create device from invalid instance or GPU");
		return;
	}
	m_gpu = m_instance->gpus()[gpu.index];
	m_physicalDevice = m_instance->physicalDevice(gpu.index);
	if (!m_physicalDevice || queueIdx >= m_gpu.queueFamilies.size())
	{
		m_error = acm::Error("failed to create device from invalid queue family");
		return;
	}
	m_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	vkGetPhysicalDeviceProperties2(m_physicalDevice, &m_properties);

	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo queueCreateInfo = {};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = queueIdx;
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = &queuePriority;

	std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

	uint32_t deviceExtensionCount = 0;
	vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &deviceExtensionCount, nullptr);
	std::vector<VkExtensionProperties> availableDeviceExtensions(deviceExtensionCount);
	vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &deviceExtensionCount, availableDeviceExtensions.data());
	for (const auto& extension : availableDeviceExtensions)
	{
		if (std::strcmp(extension.extensionName, "VK_KHR_portability_subset") == 0)
		{
			deviceExtensions.push_back("VK_KHR_portability_subset");
			break;
		}
	}

	VkPhysicalDeviceVulkan13Features vulkan13Features = {};
	vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	vulkan13Features.synchronization2 = VK_TRUE;
	vulkan13Features.dynamicRendering = VK_TRUE;
	VkPhysicalDeviceFeatures2 deviceFeatures = {};
	deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	deviceFeatures.pNext = &vulkan13Features;
	deviceFeatures.features.fillModeNonSolid = m_gpu.features.fillModeNonSolid ? VK_TRUE : VK_FALSE;
	deviceFeatures.features.wideLines = m_gpu.features.wideLines ? VK_TRUE : VK_FALSE;
	deviceFeatures.features.samplerAnisotropy = m_gpu.features.samplerAnisotropy ? VK_TRUE : VK_FALSE;
	deviceFeatures.features.sampleRateShading = m_gpu.features.sampleRateShading ? VK_TRUE : VK_FALSE;

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = &deviceFeatures;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.enabledExtensionCount = uint32_t(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

	if (vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device) != VK_SUCCESS)
	{
		m_error = acm::Error("failed to create device");
		return;
	}

	vkGetDeviceQueue(m_device, queueIdx, 0, &m_queue);
	m_queueIndex = queueIdx;
	m_enabledFeatures = m_gpu.features;
	m_allocator = std::make_unique<acm::vulkan::MemoryAllocator>(m_device, m_physicalDevice);
}

acm::vulkan::Device::~Device()
{
	if (!m_device)
		return;

	vkDeviceWaitIdle(m_device);
	clearResourcePool(m_renderers);
	clearResourcePool(m_commandBuffers);
	clearResourcePool(m_commandPools);
	clearResourcePool(m_swapChains);
	clearResourcePool(m_computePipelines);
	clearResourcePool(m_pipelines);
	clearResourcePool(m_descriptorSets);
	clearResourcePool(m_renderTargets);
	clearResourcePool(m_descriptorSetLayouts);
	clearResourcePool(m_shaders);
	clearResourcePool(m_samplers);
	clearResourcePool(m_textures);
	clearResourcePool(m_buffers);
	for (;;)
	{
		collectGarbage(UINT64_MAX);
		if (m_deferredDestroy.empty())
			break;
	}

	m_allocator.reset();
	vkDestroyDevice(m_device, nullptr);
}

VkSampleCountFlagBits acm::vulkan::Device::sampleCount(acm::SampleCount requested) const
{
	uint32_t wanted = 1;
	switch (requested)
	{
		case acm::SampleCount::One:
			wanted = 1;
			break;
		case acm::SampleCount::Two:
			wanted = 2;
			break;
		case acm::SampleCount::Four:
			wanted = 4;
			break;
		case acm::SampleCount::Eight:
			wanted = 8;
			break;
	}

	const VkSampleCountFlags supported = properties().limits.framebufferColorSampleCounts & properties().limits.framebufferDepthSampleCounts;
	for (uint32_t samples = wanted; samples >= 2; samples >>= 1)
		if (supported & samples)
			return VkSampleCountFlagBits(samples);
	return VK_SAMPLE_COUNT_1_BIT;
}

acm::SampleCount acm::vulkan::Device::maxSampleCount() const
{
	const VkSampleCountFlags supported = properties().limits.framebufferColorSampleCounts & properties().limits.framebufferDepthSampleCounts;
	if (supported & VK_SAMPLE_COUNT_8_BIT)
		return acm::SampleCount::Eight;
	if (supported & VK_SAMPLE_COUNT_4_BIT)
		return acm::SampleCount::Four;
	if (supported & VK_SAMPLE_COUNT_2_BIT)
		return acm::SampleCount::Two;
	return acm::SampleCount::One;
}

size_t acm::vulkan::Device::minUniformBufferOffsetAlignment() const
{
	return size_t(properties().limits.minUniformBufferOffsetAlignment);
}

void acm::vulkan::Device::waitIdle()
{
	vkDeviceWaitIdle(m_device);
	collectGarbage(m_lastSubmittedSerial.load(std::memory_order_relaxed));
}

void acm::vulkan::Device::collectGarbage(uint64_t completedSerial)
{
	collectResourcePool(m_renderers);
	collectResourcePool(m_commandBuffers);
	collectResourcePool(m_commandPools);
	collectResourcePool(m_swapChains);
	collectResourcePool(m_computePipelines);
	collectResourcePool(m_pipelines);
	collectResourcePool(m_descriptorSets);
	collectResourcePool(m_renderTargets);
	collectResourcePool(m_descriptorSetLayouts);
	collectResourcePool(m_shaders);
	collectResourcePool(m_samplers);
	collectResourcePool(m_textures);
	collectResourcePool(m_buffers);
	m_deferredDestroy.collect(completedSerial);
}

acm::Error acm::vulkan::Device::constructionError(acm::Error error, const char* fallback)
{
	return error ? std::move(error) : acm::Error(fallback);
}

acm::Buffer acm::vulkan::Device::createBuffer(size_t size, acm::BufferUsage usage)
{
	auto inserted = emplaceResource(m_buffers, [this, size, usage]
									{ return acm::vulkan::Buffer(*this, size, usage); });
	if (!inserted.valid())
		return acm::Buffer(constructionError(std::move(inserted.error), "failed to create buffer"));
	return acm::Buffer(std::move(inserted.resource));
}

acm::Error acm::vulkan::Device::copyBuffer(VkBuffer source, VkBuffer destination, VkDeviceSize size)
{
	return submitOneShot([source, destination, size](VkCommandBuffer commandBuffer)
						 {
		VkBufferCopy region = {};
		region.size = size;
		vkCmdCopyBuffer(commandBuffer, source, destination, 1, &region); });
}

acm::Texture acm::vulkan::Device::createTexture(acm::Format format, acm::Extent2D extent, bool mipmapped, bool storage)
{
	auto inserted = emplaceResource(m_textures, [this, format, extent, mipmapped, storage]
									{ return acm::vulkan::Texture(*this, format, extent, mipmapped, storage); });
	if (!inserted.valid())
		return acm::Texture(constructionError(std::move(inserted.error), "failed to create texture"));
	return acm::Texture(std::move(inserted.resource));
}

VkResult acm::vulkan::Device::queueSubmit(VkCommandBuffer commandBuffer, const VkSemaphoreSubmitInfo* waitSemaphore, const VkSemaphoreSubmitInfo* signalSemaphore, VkFence fence, uint64_t& submittedSerial)
{
	VkCommandBufferSubmitInfo commandBufferInfo = {};
	commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	commandBufferInfo.commandBuffer = commandBuffer;
	VkSubmitInfo2 submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo.waitSemaphoreInfoCount = waitSemaphore ? 1u : 0u;
	submitInfo.pWaitSemaphoreInfos = waitSemaphore;
	submitInfo.commandBufferInfoCount = 1;
	submitInfo.pCommandBufferInfos = &commandBufferInfo;
	submitInfo.signalSemaphoreInfoCount = signalSemaphore ? 1u : 0u;
	submitInfo.pSignalSemaphoreInfos = signalSemaphore;
	submittedSerial = m_lastSubmittedSerial.fetch_add(1, std::memory_order_relaxed) + 1;
	return vkQueueSubmit2(m_queue, 1, &submitInfo, fence);
}

acm::Error acm::vulkan::Device::submitOneShot(const std::function<void(VkCommandBuffer)>& record)
{
	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	poolInfo.queueFamilyIndex = m_queueIndex;
	VkCommandPool pool = VK_NULL_HANDLE;
	if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &pool) != VK_SUCCESS)
		return acm::Error("failed to create one-shot command pool");

	VkCommandBufferAllocateInfo allocationInfo = {};
	allocationInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocationInfo.commandPool = pool;
	allocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocationInfo.commandBufferCount = 1;
	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	if (vkAllocateCommandBuffers(m_device, &allocationInfo, &commandBuffer) != VK_SUCCESS)
	{
		vkDestroyCommandPool(m_device, pool, nullptr);
		return acm::Error("failed to allocate one-shot command buffer");
	}

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
	{
		vkDestroyCommandPool(m_device, pool, nullptr);
		return acm::Error("failed to begin one-shot command buffer");
	}
	record(commandBuffer);
	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
	{
		vkDestroyCommandPool(m_device, pool, nullptr);
		return acm::Error("failed to end one-shot command buffer");
	}

	std::lock_guard<std::mutex> lock(m_queueMutex);
	uint64_t submittedSerial = 0;
	if (queueSubmit(commandBuffer, nullptr, nullptr, VK_NULL_HANDLE, submittedSerial) != VK_SUCCESS)
	{
		vkDestroyCommandPool(m_device, pool, nullptr);
		return acm::Error("failed to submit one-shot command buffer");
	}
	if (vkQueueWaitIdle(m_queue) != VK_SUCCESS)
	{
		vkDestroyCommandPool(m_device, pool, nullptr);
		return acm::Error("failed to wait for one-shot command buffer");
	}
	collectGarbage(submittedSerial);
	vkDestroyCommandPool(m_device, pool, nullptr);
	return {};
}

acm::Error acm::vulkan::Device::submitFrame(VkCommandBuffer commandBuffer, VkSemaphore imageAvailable, VkSemaphore renderFinished, VkFence inFlight, VkSwapchainKHR swapChain, uint32_t imageIndex, bool& needsRecreate, uint64_t& submittedSerial)
{
	VkSemaphoreSubmitInfo waitSemaphore = {};
	waitSemaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	waitSemaphore.semaphore = imageAvailable;
	waitSemaphore.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSemaphoreSubmitInfo signalSemaphore = {};
	signalSemaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signalSemaphore.semaphore = renderFinished;
	signalSemaphore.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinished;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapChain;
	presentInfo.pImageIndices = &imageIndex;

	std::lock_guard<std::mutex> lock(m_queueMutex);
	if (vkResetFences(m_device, 1, &inFlight) != VK_SUCCESS)
		return acm::Error("failed to reset draw fence");
	if (queueSubmit(commandBuffer, &waitSemaphore, &signalSemaphore, inFlight, submittedSerial) != VK_SUCCESS)
		return acm::Error("failed to submit draw command buffer");
	const VkResult present = vkQueuePresentKHR(m_queue, &presentInfo);
	if (present == VK_ERROR_OUT_OF_DATE_KHR || present == VK_SUBOPTIMAL_KHR)
		needsRecreate = true;
	else if (present != VK_SUCCESS)
		return acm::Error("failed to present swapchain image");
	return {};
}

acm::DescriptorSetLayout acm::vulkan::Device::createDescriptorSetLayout(const std::vector<acm::DescriptorBinding>& bindings)
{
	auto inserted = emplaceResource(m_descriptorSetLayouts, [this, &bindings]
									{ return acm::vulkan::DescriptorSetLayout(*this, bindings); });
	if (!inserted.valid())
		return acm::DescriptorSetLayout(constructionError(std::move(inserted.error), "failed to create descriptor set layout"));
	return acm::DescriptorSetLayout(std::move(inserted.resource));
}

acm::DescriptorSet acm::vulkan::Device::createDescriptorSet(const acm::DescriptorSetLayout& layout)
{
	if (!layout.valid() || &layout.native()->owner() != this)
		return acm::DescriptorSet(acm::Error("failed to create descriptor set from invalid layout"));
	auto inserted = emplaceResource(m_descriptorSets, [this, &layout]
									{ return acm::vulkan::DescriptorSet(*this, layout); });
	if (!inserted.valid())
		return acm::DescriptorSet(constructionError(std::move(inserted.error), "failed to create descriptor set"));
	return acm::DescriptorSet(std::move(inserted.resource));
}

acm::Pipeline acm::vulkan::Device::createPipeline(const acm::PipelineConfig& config)
{
	auto inserted = emplaceResource(m_pipelines, [this, &config]
									{ return acm::vulkan::Pipeline(*this, config); });
	if (!inserted.valid())
		return acm::Pipeline(constructionError(std::move(inserted.error), "failed to create pipeline"));
	return acm::Pipeline(std::move(inserted.resource));
}

acm::ComputePipeline acm::vulkan::Device::createComputePipeline(const acm::Shader& compute, const acm::DescriptorSetLayout& layout)
{
	if (!compute.valid() || &compute.native()->owner() != this)
		return acm::ComputePipeline(acm::Error("failed to create compute pipeline from invalid shader"));
	if (layout.valid() && &layout.native()->owner() != this)
		return acm::ComputePipeline(acm::Error("failed to create compute pipeline from invalid descriptor layout"));
	auto inserted = emplaceResource(m_computePipelines, [this, &compute, &layout]
									{ return acm::vulkan::ComputePipeline(*this, compute, layout); });
	if (!inserted.valid())
		return acm::ComputePipeline(constructionError(std::move(inserted.error), "failed to create compute pipeline"));
	return acm::ComputePipeline(std::move(inserted.resource));
}

acm::RenderTarget acm::vulkan::Device::createRenderTarget(VkImage image, acm::Format format, acm::Extent2D extent, bool depth, acm::SampleCount samples)
{
	auto inserted = emplaceResource(m_renderTargets, [this, image, format, extent, depth, samples]
									{ return acm::vulkan::RenderTarget(*this, image, format, extent, depth, samples); });
	if (!inserted.valid())
		return acm::RenderTarget(constructionError(std::move(inserted.error), "failed to create render target"));
	return acm::RenderTarget(std::move(inserted.resource));
}

acm::RenderTarget acm::vulkan::Device::createRenderTarget(const acm::Texture& texture, acm::RenderTargetFinish finish, bool depth, acm::SampleCount samples)
{
	if (!texture.valid() || &texture.native()->owner() != this)
		return acm::RenderTarget(acm::Error("failed to create render target from invalid texture"));
	auto inserted = emplaceResource(m_renderTargets, [this, &texture, finish, depth, samples]
									{ return acm::vulkan::RenderTarget(*this, texture, finish, depth, samples); });
	if (!inserted.valid())
		return acm::RenderTarget(constructionError(std::move(inserted.error), "failed to create render target"));
	return acm::RenderTarget(std::move(inserted.resource));
}

bool acm::vulkan::Device::invalidateRenderTarget(acm::RenderTarget& target)
{
	return target.m_resource.forceInvalidate();
}

acm::SwapChain acm::vulkan::Device::createSwapChain(const acm::Surface& surface, acm::SurfaceFormat format, acm::PresentMode presentMode, acm::Extent2D desiredExtent, bool depth, acm::SampleCount samples)
{
	if (!surface.valid() || &surface.native()->owner() != m_instance)
		return acm::SwapChain(acm::Error("failed to create swapchain from invalid surface"));
	auto inserted = emplaceResource(m_swapChains, [this, &surface, format, presentMode, desiredExtent, depth, samples]
									{ return acm::vulkan::SwapChain(*this, surface, format, presentMode, desiredExtent, depth, samples); });
	if (!inserted.valid())
		return acm::SwapChain(constructionError(std::move(inserted.error), "failed to create swapchain"));
	return acm::SwapChain(std::move(inserted.resource));
}

acm::CommandPool acm::vulkan::Device::createCommandPool()
{
	auto inserted = emplaceResource(m_commandPools, [this]
									{ return acm::vulkan::CommandPool(*this); });
	if (!inserted.valid())
		return acm::CommandPool(constructionError(std::move(inserted.error), "failed to create command pool"));
	return acm::CommandPool(std::move(inserted.resource));
}

acm::CommandBuffer acm::vulkan::Device::allocateCommandBuffer(const acm::CommandPool& pool)
{
	if (!pool.valid() || &pool.native()->owner() != this)
		return acm::CommandBuffer(acm::Error("failed to allocate from invalid command pool"));

	auto inserted = emplaceResource(m_commandBuffers, [this, &pool]
									{ return acm::vulkan::CommandBuffer(*this, pool); });
	if (!inserted.valid())
		return acm::CommandBuffer(constructionError(std::move(inserted.error), "failed to allocate command buffer"));
	return acm::CommandBuffer(std::move(inserted.resource));
}

acm::Error acm::vulkan::Device::submitCommandBufferSync(const acm::CommandBuffer& commandBuffer)
{
	if (!commandBuffer.valid() || &commandBuffer.native()->owner() != this)
		return acm::Error("submitSync: invalid command buffer");
	VkCommandBuffer vkCommand = commandBuffer.native()->vkCommandBuffer();
	std::lock_guard<std::mutex> lock(m_queueMutex);
	uint64_t submittedSerial = 0;
	if (queueSubmit(vkCommand, nullptr, nullptr, VK_NULL_HANDLE, submittedSerial) != VK_SUCCESS)
		return acm::Error("submitSync: failed to submit");
	if (vkQueueWaitIdle(m_queue) != VK_SUCCESS)
		return acm::Error("submitSync: failed to wait for queue");
	collectGarbage(submittedSerial);
	return {};
}

acm::Renderer acm::vulkan::Device::createRenderer(const acm::SwapChain& swapChain)
{
	if (!swapChain.valid() || &swapChain.native()->owner() != this)
		return acm::Renderer(acm::Error("failed to create renderer from invalid swapchain"));
	auto inserted = emplaceResource(m_renderers, [this, &swapChain]
									{ return acm::vulkan::Renderer(*this, swapChain); });
	if (!inserted.valid())
		return acm::Renderer(constructionError(std::move(inserted.error), "failed to create renderer"));
	return acm::Renderer(std::move(inserted.resource));
}

acm::Sampler acm::vulkan::Device::createSampler(float maxAnisotropy)
{
	auto inserted = emplaceResource(m_samplers, [this, maxAnisotropy]
									{ return acm::vulkan::Sampler(*this, maxAnisotropy); });
	if (!inserted.valid())
		return acm::Sampler(constructionError(std::move(inserted.error), "failed to create sampler"));
	return acm::Sampler(std::move(inserted.resource));
}

acm::Shader acm::vulkan::Device::createShader(const std::vector<char>& spirv)
{
	auto inserted = emplaceResource(m_shaders, [this, &spirv]
									{ return acm::vulkan::Shader(*this, spirv); });
	if (!inserted.valid())
		return acm::Shader(constructionError(std::move(inserted.error), "failed to create shader module"));
	return acm::Shader(std::move(inserted.resource));
}
