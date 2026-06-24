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
#include "archimedes/vulkan/Convert.h"
#include "archimedes/vulkan/Instance.h"

#include <cstring>
#include <utility>

acm::vulkan::Device::Device(acm::vulkan::Instance& instance, const acm::GPU& gpu, uint32_t queueIdx)
	: m_instance(&instance)
	, m_buffers(*this)
	, m_textures(*this)
	, m_samplers(*this)
	, m_shaders(*this)
	, m_descriptorSetLayouts(*this)
	, m_descriptorSets(*this)
	, m_pipelines(*this)
	, m_computePipelines(*this)
	, m_renderTargets(*this)
	, m_swapChains(*this)
	, m_commandPools(*this)
	, m_commandBuffers(*this)
	, m_renderers(*this)
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

	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo queueCreateInfo = {};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = queueIdx;
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = &queuePriority;

	const std::vector<const char*>& layerNames = m_instance->layerNames();
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

	VkPhysicalDeviceFeatures deviceFeatures = {};
	deviceFeatures.fillModeNonSolid = m_gpu.features.fillModeNonSolid ? VK_TRUE : VK_FALSE;
	deviceFeatures.wideLines = m_gpu.features.wideLines ? VK_TRUE : VK_FALSE;
	deviceFeatures.samplerAnisotropy = m_gpu.features.samplerAnisotropy ? VK_TRUE : VK_FALSE;
	deviceFeatures.sampleRateShading = m_gpu.features.sampleRateShading ? VK_TRUE : VK_FALSE;

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.enabledLayerCount = uint32_t(layerNames.size());
	deviceCreateInfo.ppEnabledLayerNames = layerNames.data();
	deviceCreateInfo.enabledExtensionCount = uint32_t(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

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
	m_renderers.clear();
	m_commandBuffers.clear();
	m_commandPools.clear();
	m_swapChains.clear();
	m_computePipelines.clear();
	m_pipelines.clear();
	m_descriptorSets.clear();
	m_renderTargets.clear();
	m_descriptorSetLayouts.clear();
	m_shaders.clear();
	m_samplers.clear();
	m_textures.clear();
	m_buffers.clear();
	for (;;)
	{
		collectGarbage(UINT64_MAX);
		std::lock_guard<std::mutex> lock(m_graveyardMutex);
		if (m_graveyard.empty())
			break;
	}

	m_allocator.reset();
	vkDestroyDevice(m_device, nullptr);
}

acm::SampleCount acm::vulkan::Device::maxSampleCount() const
{
	return acm::vulkan::maxSampleCount(m_physicalDevice);
}

size_t acm::vulkan::Device::minUniformBufferOffsetAlignment() const
{
	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);
	return size_t(properties.limits.minUniformBufferOffsetAlignment);
}

void acm::vulkan::Device::waitIdle()
{
	vkDeviceWaitIdle(m_device);
}

void acm::vulkan::Device::beginFrame()
{
	m_currentFrame.fetch_add(1, std::memory_order_relaxed);
}

void acm::vulkan::Device::enqueueDestroy(std::function<void()> destroy)
{
	std::lock_guard<std::mutex> lock(m_graveyardMutex);
	m_graveyard.push_back({m_currentFrame.load(std::memory_order_relaxed), std::move(destroy)});
}

void acm::vulkan::Device::collectGarbage(uint64_t completedFrame)
{
	std::vector<std::function<void()>> ready;
	{
		std::lock_guard<std::mutex> lock(m_graveyardMutex);
		auto pending = m_graveyard.begin();
		while (pending != m_graveyard.end())
		{
			if (pending->frame <= completedFrame)
			{
				ready.push_back(std::move(pending->destroy));
				pending = m_graveyard.erase(pending);
			}
			else
			{
				++pending;
			}
		}
	}
	for (auto& destroy : ready)
		destroy();
}

acm::Buffer acm::vulkan::Device::createBuffer(size_t size, acm::BufferUsage usage)
{
	auto inserted = m_buffers.emplace([this, size, usage](acm::vulkan::Buffer& buffer)
									  { return buffer.create(*this, size, usage); });
	if (!inserted.resource)
		return acm::Buffer(acm::Error("failed to create buffer"));
	return acm::Buffer(inserted.resource, inserted.handle);
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
	auto inserted = m_textures.emplace([this, format, extent, mipmapped, storage](acm::vulkan::Texture& texture)
									   { return texture.create(*this, format, extent, mipmapped, storage); });
	if (!inserted.resource)
		return acm::Texture(acm::Error("failed to create texture"));
	return acm::Texture(inserted.resource, inserted.handle);
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

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	std::lock_guard<std::mutex> lock(m_queueMutex);
	if (vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
	{
		vkDestroyCommandPool(m_device, pool, nullptr);
		return acm::Error("failed to submit one-shot command buffer");
	}
	vkQueueWaitIdle(m_queue);
	vkDestroyCommandPool(m_device, pool, nullptr);
	return {};
}

acm::Error acm::vulkan::Device::submitFrame(VkCommandBuffer commandBuffer, VkSemaphore imageAvailable, VkSemaphore renderFinished, VkFence inFlight, VkSwapchainKHR swapChain, uint32_t imageIndex, bool& needsRecreate)
{
	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &imageAvailable;
	submitInfo.pWaitDstStageMask = &waitStage;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderFinished;
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinished;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapChain;
	presentInfo.pImageIndices = &imageIndex;

	std::lock_guard<std::mutex> lock(m_queueMutex);
	vkResetFences(m_device, 1, &inFlight);
	if (vkQueueSubmit(m_queue, 1, &submitInfo, inFlight) != VK_SUCCESS)
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
	auto inserted = m_descriptorSetLayouts.emplace([this, &bindings](acm::vulkan::DescriptorSetLayout& layout)
												   { return layout.create(*this, bindings); });
	if (!inserted.resource)
		return acm::DescriptorSetLayout(acm::Error("failed to create descriptor set layout"));
	return acm::DescriptorSetLayout(inserted.resource, inserted.handle);
}

acm::DescriptorSet acm::vulkan::Device::createDescriptorSet(const acm::DescriptorSetLayout& layout)
{
	if (!layout.valid() || &layout.native()->owner() != this)
		return acm::DescriptorSet(acm::Error("failed to create descriptor set from invalid layout"));
	auto inserted = m_descriptorSets.emplace([this, &layout](acm::vulkan::DescriptorSet& descriptorSet)
											 { return descriptorSet.create(*this, *layout.native(), layout.handle()); });
	if (!inserted.resource)
		return acm::DescriptorSet(acm::Error("failed to create descriptor set"));
	return acm::DescriptorSet(inserted.resource, inserted.handle);
}

acm::Pipeline acm::vulkan::Device::createPipeline(const acm::PipelineConfig& config)
{
	auto inserted = m_pipelines.emplace([this, &config](acm::vulkan::Pipeline& pipeline)
										{ return pipeline.create(*this, config); });
	if (!inserted.resource)
		return acm::Pipeline(acm::Error("failed to create pipeline"));
	return acm::Pipeline(inserted.resource, inserted.handle);
}

acm::ComputePipeline acm::vulkan::Device::createComputePipeline(const acm::Shader& compute, const acm::DescriptorSetLayout& layout)
{
	if (!compute.valid() || &compute.native()->owner() != this)
		return acm::ComputePipeline(acm::Error("failed to create compute pipeline from invalid shader"));
	if (layout.valid() && &layout.native()->owner() != this)
		return acm::ComputePipeline(acm::Error("failed to create compute pipeline from invalid descriptor layout"));
	auto inserted = m_computePipelines.emplace([this, &compute, &layout](acm::vulkan::ComputePipeline& pipeline)
											   { return pipeline.create(*this, *compute.native(), compute.handle(), layout.valid() ? layout.native() : nullptr, layout.handle()); });
	if (!inserted.resource)
		return acm::ComputePipeline(acm::Error("failed to create compute pipeline"));
	return acm::ComputePipeline(inserted.resource, inserted.handle);
}

acm::RenderTarget acm::vulkan::Device::createRenderTarget(VkRenderPass renderPass, VkImage image, acm::Format format, acm::Extent2D extent, bool depth, acm::SampleCount samples)
{
	auto inserted = m_renderTargets.emplace([this, renderPass, image, format, extent, depth, samples](acm::vulkan::RenderTarget& target)
											{ return target.create(*this, renderPass, image, format, extent, depth, samples); });
	if (!inserted.resource)
		return acm::RenderTarget(acm::Error("failed to create render target"));
	return acm::RenderTarget(inserted.resource, inserted.handle);
}

acm::RenderTarget acm::vulkan::Device::createRenderTarget(const acm::Texture& texture, acm::RenderTargetFinish finish, bool depth, acm::SampleCount samples)
{
	if (!texture.valid() || &texture.native()->owner() != this)
		return acm::RenderTarget(acm::Error("failed to create render target from invalid texture"));
	auto inserted = m_renderTargets.emplace([this, &texture, finish, depth, samples](acm::vulkan::RenderTarget& target)
											{ return target.create(*this, *texture.native(), texture.handle(), finish, depth, samples); });
	if (!inserted.resource)
		return acm::RenderTarget(acm::Error("failed to create render target"));
	return acm::RenderTarget(inserted.resource, inserted.handle);
}

acm::SwapChain acm::vulkan::Device::createSwapChain(const acm::Surface& surface, acm::SurfaceFormat format, acm::PresentMode presentMode, acm::Extent2D desiredExtent, bool depth, acm::SampleCount samples)
{
	if (!surface.valid() || &surface.native()->owner() != m_instance)
		return acm::SwapChain(acm::Error("failed to create swapchain from invalid surface"));
	auto inserted = m_swapChains.emplace([this, &surface, format, presentMode, desiredExtent, depth, samples](acm::vulkan::SwapChain& swapChain)
										 { return swapChain.create(*this, *surface.native(), surface.handle(), format, presentMode, desiredExtent, depth, samples); });
	if (!inserted.resource)
		return acm::SwapChain(acm::Error("failed to create swapchain"));
	return acm::SwapChain(inserted.resource, inserted.handle);
}

acm::CommandPool acm::vulkan::Device::createCommandPool()
{
	auto inserted = m_commandPools.emplace([this](acm::vulkan::CommandPool& commandPool)
										   { return commandPool.create(*this); });
	if (!inserted.resource)
		return acm::CommandPool(acm::Error("failed to create command pool"));
	return acm::CommandPool(inserted.resource, inserted.handle);
}

acm::CommandBuffer acm::vulkan::Device::allocateCommandBuffer(acm::vulkan::CommandPool* poolResource, const acm::Handle& pool)
{
	if (!poolResource || &poolResource->owner() != this || !poolResource->valid(pool))
		return acm::CommandBuffer(acm::Error("failed to allocate from invalid command pool"));

	auto inserted = m_commandBuffers.emplace([this, poolResource, pool](acm::vulkan::CommandBuffer& commandBuffer)
											 { return commandBuffer.create(*this, *poolResource, pool); });
	if (!inserted.resource)
		return acm::CommandBuffer(acm::Error("failed to retain command pool"));
	return acm::CommandBuffer(inserted.resource, inserted.handle);
}

acm::Error acm::vulkan::Device::submitCommandBufferSync(const acm::CommandBuffer& commandBuffer)
{
	if (!commandBuffer.valid() || &commandBuffer.native()->owner() != this)
		return acm::Error("submitSync: invalid command buffer");
	VkCommandBuffer vkCommand = commandBuffer.native()->vkCommandBuffer(commandBuffer.handle());
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &vkCommand;
	std::lock_guard<std::mutex> lock(m_queueMutex);
	if (vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
		return acm::Error("submitSync: failed to submit");
	vkQueueWaitIdle(m_queue);
	return {};
}

acm::Renderer acm::vulkan::Device::createRenderer(const acm::SwapChain& swapChain)
{
	if (!swapChain.valid() || &swapChain.native()->owner() != this)
		return acm::Renderer(acm::Error("failed to create renderer from invalid swapchain"));
	auto inserted = m_renderers.emplace([this, &swapChain](acm::vulkan::Renderer& renderer)
										{ return renderer.create(*this, swapChain); });
	if (!inserted.resource)
		return acm::Renderer(acm::Error("failed to create renderer"));
	return acm::Renderer(inserted.resource, inserted.handle);
}

acm::Sampler acm::vulkan::Device::createSampler(float maxAnisotropy)
{
	auto inserted = m_samplers.emplace([this, maxAnisotropy](acm::vulkan::Sampler& sampler)
									   { return sampler.create(*this, maxAnisotropy); });
	if (!inserted.resource)
		return acm::Sampler(acm::Error("failed to create sampler"));
	return acm::Sampler(inserted.resource, inserted.handle);
}

acm::Shader acm::vulkan::Device::createShader(const std::vector<char>& spirv)
{
	auto inserted = m_shaders.emplace([this, &spirv](acm::vulkan::Shader& shader)
									  { return shader.create(*this, spirv); });
	if (!inserted.resource)
		return acm::Shader(acm::Error("failed to create shader module"));
	return acm::Shader(inserted.resource, inserted.handle);
}
