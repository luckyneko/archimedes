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
	, m_buffers([this](auto& resource) { resource.retire(*this); })
	, m_textures([this](auto& resource) { resource.retire(*this); })
	, m_samplers([this](auto& resource) { resource.retire(*this); })
	, m_shaders([this](auto& resource) { resource.retire(*this); })
	, m_descriptorSetLayouts([this](auto& resource) { resource.retire(*this); })
	, m_descriptorSets([this](auto& resource) { resource.retire(*this); })
	, m_pipelines([this](auto& resource) { resource.retire(*this); })
	, m_computePipelines([this](auto& resource) { resource.retire(*this); })
	, m_renderTargets([this](auto& resource) { resource.retire(*this); })
	, m_swapChains([this](auto& resource) { resource.retire(*this); })
	, m_commandPools([this](auto& resource) { resource.retire(*this); })
	, m_commandBuffers([this](auto& resource) { resource.retire(*this); })
	, m_renderers([this](auto& resource) { resource.retire(*this); })
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
	if (!inserted.valid())
		return acm::Buffer(acm::Error("failed to create buffer"));
	return acm::Buffer(std::move(inserted));
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
	if (!inserted.valid())
		return acm::Texture(acm::Error("failed to create texture"));
	return acm::Texture(std::move(inserted));
}

VkResult acm::vulkan::Device::queueSubmit(VkCommandBuffer commandBuffer, const VkSemaphoreSubmitInfo* waitSemaphore, const VkSemaphoreSubmitInfo* signalSemaphore, VkFence fence)
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
	if (queueSubmit(commandBuffer, nullptr, nullptr, VK_NULL_HANDLE) != VK_SUCCESS)
	{
		vkDestroyCommandPool(m_device, pool, nullptr);
		return acm::Error("failed to submit one-shot command buffer");
	}
	if (vkQueueWaitIdle(m_queue) != VK_SUCCESS)
	{
		vkDestroyCommandPool(m_device, pool, nullptr);
		return acm::Error("failed to wait for one-shot command buffer");
	}
	vkDestroyCommandPool(m_device, pool, nullptr);
	return {};
}

acm::Error acm::vulkan::Device::submitFrame(VkCommandBuffer commandBuffer, VkSemaphore imageAvailable, VkSemaphore renderFinished, VkFence inFlight, VkSwapchainKHR swapChain, uint32_t imageIndex, bool& needsRecreate)
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
	if (queueSubmit(commandBuffer, &waitSemaphore, &signalSemaphore, inFlight) != VK_SUCCESS)
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
	if (!inserted.valid())
		return acm::DescriptorSetLayout(acm::Error("failed to create descriptor set layout"));
	return acm::DescriptorSetLayout(std::move(inserted));
}

acm::DescriptorSet acm::vulkan::Device::createDescriptorSet(const acm::DescriptorSetLayout& layout)
{
	if (!layout.valid() || &layout.native()->owner() != this)
		return acm::DescriptorSet(acm::Error("failed to create descriptor set from invalid layout"));
	auto inserted = m_descriptorSets.emplace([this, &layout](acm::vulkan::DescriptorSet& descriptorSet)
											 { return descriptorSet.create(*this, layout); });
	if (!inserted.valid())
		return acm::DescriptorSet(acm::Error("failed to create descriptor set"));
	return acm::DescriptorSet(std::move(inserted));
}

acm::Pipeline acm::vulkan::Device::createPipeline(const acm::PipelineConfig& config)
{
	auto inserted = m_pipelines.emplace([this, &config](acm::vulkan::Pipeline& pipeline)
										{ return pipeline.create(*this, config); });
	if (!inserted.valid())
		return acm::Pipeline(acm::Error("failed to create pipeline"));
	return acm::Pipeline(std::move(inserted));
}

acm::ComputePipeline acm::vulkan::Device::createComputePipeline(const acm::Shader& compute, const acm::DescriptorSetLayout& layout)
{
	if (!compute.valid() || &compute.native()->owner() != this)
		return acm::ComputePipeline(acm::Error("failed to create compute pipeline from invalid shader"));
	if (layout.valid() && &layout.native()->owner() != this)
		return acm::ComputePipeline(acm::Error("failed to create compute pipeline from invalid descriptor layout"));
	auto inserted = m_computePipelines.emplace([this, &compute, &layout](acm::vulkan::ComputePipeline& pipeline)
											   { return pipeline.create(*this, compute, layout); });
	if (!inserted.valid())
		return acm::ComputePipeline(acm::Error("failed to create compute pipeline"));
	return acm::ComputePipeline(std::move(inserted));
}

acm::RenderTarget acm::vulkan::Device::createRenderTarget(VkImage image, acm::Format format, acm::Extent2D extent, bool depth, acm::SampleCount samples)
{
	auto inserted = m_renderTargets.emplace([this, image, format, extent, depth, samples](acm::vulkan::RenderTarget& target)
											{ return target.create(*this, image, format, extent, depth, samples); });
	if (!inserted.valid())
		return acm::RenderTarget(acm::Error("failed to create render target"));
	return acm::RenderTarget(std::move(inserted));
}

acm::RenderTarget acm::vulkan::Device::createRenderTarget(const acm::Texture& texture, acm::RenderTargetFinish finish, bool depth, acm::SampleCount samples)
{
	if (!texture.valid() || &texture.native()->owner() != this)
		return acm::RenderTarget(acm::Error("failed to create render target from invalid texture"));
	auto inserted = m_renderTargets.emplace([this, &texture, finish, depth, samples](acm::vulkan::RenderTarget& target)
											{ return target.create(*this, texture, finish, depth, samples); });
	if (!inserted.valid())
		return acm::RenderTarget(acm::Error("failed to create render target"));
	return acm::RenderTarget(std::move(inserted));
}

bool acm::vulkan::Device::invalidateRenderTarget(acm::RenderTarget& target)
{
	return target.m_resource.forceInvalidate();
}

acm::SwapChain acm::vulkan::Device::createSwapChain(const acm::Surface& surface, acm::SurfaceFormat format, acm::PresentMode presentMode, acm::Extent2D desiredExtent, bool depth, acm::SampleCount samples)
{
	if (!surface.valid() || &surface.native()->owner() != m_instance)
		return acm::SwapChain(acm::Error("failed to create swapchain from invalid surface"));
	auto inserted = m_swapChains.emplace([this, &surface, format, presentMode, desiredExtent, depth, samples](acm::vulkan::SwapChain& swapChain)
										 { return swapChain.create(*this, surface, format, presentMode, desiredExtent, depth, samples); });
	if (!inserted.valid())
		return acm::SwapChain(acm::Error("failed to create swapchain"));
	return acm::SwapChain(std::move(inserted));
}

acm::CommandPool acm::vulkan::Device::createCommandPool()
{
	auto inserted = m_commandPools.emplace([this](acm::vulkan::CommandPool& commandPool)
										   { return commandPool.create(*this); });
	if (!inserted.valid())
		return acm::CommandPool(acm::Error("failed to create command pool"));
	return acm::CommandPool(std::move(inserted));
}

acm::CommandBuffer acm::vulkan::Device::allocateCommandBuffer(const acm::CommandPool& pool)
{
	if (!pool.valid() || &pool.native()->owner() != this)
		return acm::CommandBuffer(acm::Error("failed to allocate from invalid command pool"));

	auto inserted = m_commandBuffers.emplace([this, &pool](acm::vulkan::CommandBuffer& commandBuffer)
											 { return commandBuffer.create(*this, pool); });
	if (!inserted.valid())
		return acm::CommandBuffer(acm::Error("failed to retain command pool"));
	return acm::CommandBuffer(std::move(inserted));
}

acm::Error acm::vulkan::Device::submitCommandBufferSync(const acm::CommandBuffer& commandBuffer)
{
	if (!commandBuffer.valid() || &commandBuffer.native()->owner() != this)
		return acm::Error("submitSync: invalid command buffer");
	VkCommandBuffer vkCommand = commandBuffer.native()->vkCommandBuffer();
	std::lock_guard<std::mutex> lock(m_queueMutex);
	if (queueSubmit(vkCommand, nullptr, nullptr, VK_NULL_HANDLE) != VK_SUCCESS)
		return acm::Error("submitSync: failed to submit");
	if (vkQueueWaitIdle(m_queue) != VK_SUCCESS)
		return acm::Error("submitSync: failed to wait for queue");
	return {};
}

acm::Renderer acm::vulkan::Device::createRenderer(const acm::SwapChain& swapChain)
{
	if (!swapChain.valid() || &swapChain.native()->owner() != this)
		return acm::Renderer(acm::Error("failed to create renderer from invalid swapchain"));
	auto inserted = m_renderers.emplace([this, &swapChain](acm::vulkan::Renderer& renderer)
										{ return renderer.create(*this, swapChain); });
	if (!inserted.valid())
		return acm::Renderer(acm::Error("failed to create renderer"));
	return acm::Renderer(std::move(inserted));
}

acm::Sampler acm::vulkan::Device::createSampler(float maxAnisotropy)
{
	auto inserted = m_samplers.emplace([this, maxAnisotropy](acm::vulkan::Sampler& sampler)
									   { return sampler.create(*this, maxAnisotropy); });
	if (!inserted.valid())
		return acm::Sampler(acm::Error("failed to create sampler"));
	return acm::Sampler(std::move(inserted));
}

acm::Shader acm::vulkan::Device::createShader(const std::vector<char>& spirv)
{
	auto inserted = m_shaders.emplace([this, &spirv](acm::vulkan::Shader& shader)
									  { return shader.create(*this, spirv); });
	if (!inserted.valid())
		return acm::Shader(acm::Error("failed to create shader module"));
	return acm::Shader(std::move(inserted));
}
