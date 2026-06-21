
#include "archimedes/acmDevice.h"

#include "archimedes/acmBuffer.h"
#include "archimedes/acmCommandBuffer.h"
#include "archimedes/acmCommandPool.h"
#include "archimedes/acmComputePipeline.h"
#include "archimedes/acmDescriptorSet.h"
#include "archimedes/acmDescriptorSetLayout.h"
#include "archimedes/acmInstance.h"
#include "archimedes/acmPipeline.h"
#include "archimedes/acmRenderer.h"
#include "archimedes/acmRenderTarget.h"
#include "archimedes/acmSampler.h"
#include "archimedes/acmShader.h"
#include "archimedes/acmSurface.h"
#include "archimedes/acmSwapChain.h"
#include "archimedes/acmTexture.h"
#include "archimedes/acmUniformRing.h"
#include "archimedes/acmVkConvert.h"
#include "archimedes/acmVkMemory.h"

#include <vulkan/vulkan.h>

#include <cstring>
#include <vector>

struct acm::Device::impl
{
	acm::Instance instance;
	acm::GPU gpu;
	uint32_t queueIdx{0};
	acm::GPUFeatures enabledFeatures;
	VkDevice device{VK_NULL_HANDLE};
	VkQueue queue{VK_NULL_HANDLE};
	std::unique_ptr<acm::MemoryAllocator> allocator;

	// Externally synchronizes the single VkQueue + the frame/graveyard bookkeeping
	// when several Renderers submit from their own threads (see Device::deviceMutex).
	std::mutex deviceMutex;

	uint64_t currentFrame{0};
	struct Pending
	{
		uint64_t frame{0};
		std::function<void()> destroy;
	};
	std::vector<Pending> graveyard;

	~impl()
	{
		if (device)
		{
			// The device is going away; nothing can still be in flight after we
			// wait, so flush every remaining deferred destroy before the device
			// itself. The lambdas only capture raw handles, so the live VkDevice
			// is valid right up until vkDestroyDevice below.
			vkDeviceWaitIdle(device);
			for (auto& entry : graveyard)
				entry.destroy();
			graveyard.clear();
			// Resources are now destroyed and their pool ranges freed; tear down the
			// allocator's blocks (vkFreeMemory) before the device they belong to.
			allocator.reset();
			vkDestroyDevice(device, nullptr);
		}
	}
};

acm::Device::Device(acm::Instance instance, const acm::GPU& gpu, uint32_t queueIdx)
	: m()
{
	auto impl = std::make_shared<acm::Device::impl>();
	impl->instance = instance;

	// Create logical device
	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo queueCreateInfo = {};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = queueIdx;
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = &queuePriority;

	const std::vector<const char*>& layerNames = instance.getLayerNames();
	std::vector<const char*> deviceExtensions =
		{
			VK_KHR_SWAPCHAIN_EXTENSION_NAME};

	// Portability devices (e.g. MoltenVK on macOS) must enable
	// VK_KHR_portability_subset whenever they advertise it. The name macro
	// lives behind VK_ENABLE_BETA_EXTENSIONS, so match the literal instead.
	uint32_t deviceExtCount = 0;
	vkEnumerateDeviceExtensionProperties(gpu.device, nullptr, &deviceExtCount, nullptr);
	std::vector<VkExtensionProperties> availableDeviceExts(deviceExtCount);
	vkEnumerateDeviceExtensionProperties(gpu.device, nullptr, &deviceExtCount, availableDeviceExts.data());
	for (const auto& ext : availableDeviceExts)
	{
		if (strcmp(ext.extensionName, "VK_KHR_portability_subset") == 0)
		{
			deviceExtensions.push_back("VK_KHR_portability_subset");
			break;
		}
	}
	// Enable the curated features this GPU supports (availability was queried at
	// enumeration). impl->enabledFeatures records what we turned on, so consumers
	// (e.g. a wireframe pipeline) can check before relying on one.
	VkPhysicalDeviceFeatures deviceFeatures = {};
	deviceFeatures.fillModeNonSolid = gpu.features.fillModeNonSolid ? VK_TRUE : VK_FALSE;
	deviceFeatures.wideLines = gpu.features.wideLines ? VK_TRUE : VK_FALSE;
	deviceFeatures.samplerAnisotropy = gpu.features.samplerAnisotropy ? VK_TRUE : VK_FALSE;
	deviceFeatures.sampleRateShading = gpu.features.sampleRateShading ? VK_TRUE : VK_FALSE;
	impl->enabledFeatures = gpu.features;

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.enabledLayerCount = uint32_t(layerNames.size());
	deviceCreateInfo.ppEnabledLayerNames = layerNames.data();
	deviceCreateInfo.enabledExtensionCount = uint32_t(deviceExtensions.size());
	;
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

	if (vkCreateDevice(gpu.device, &deviceCreateInfo, nullptr, &impl->device) != VK_SUCCESS)
		return;
	vkGetDeviceQueue(impl->device, queueIdx, 0, &impl->queue);
	impl->gpu = gpu;
	impl->queueIdx = queueIdx;
	impl->allocator = std::make_unique<acm::MemoryAllocator>(impl->device, gpu.device);

	m = impl;
}

acm::SwapChain acm::Device::createSwapChain(acm::Surface surface, acm::SurfaceFormat format, acm::PresentMode presentMode, acm::Extent2D desiredExtent, bool depth, acm::SampleCount samples)
{
	return acm::SwapChain(*this, surface, format, presentMode, desiredExtent, depth, samples);
}

acm::RenderTarget acm::Device::createRenderTarget(VkRenderPass renderPass, VkImage image, acm::Format format, acm::Extent2D extent, bool depth, acm::SampleCount samples)
{
	return acm::RenderTarget(*this, renderPass, image, format, extent, depth, samples);
}

acm::RenderTarget acm::Device::createRenderTarget(acm::Texture texture, acm::RenderTargetFinish finish, bool depth, acm::SampleCount samples)
{
	return acm::RenderTarget(*this, texture, finish, depth, samples);
}

acm::Texture acm::Device::createTexture(acm::Format format, acm::Extent2D extent, bool mipmapped, bool storage)
{
	return acm::Texture(*this, format, extent, mipmapped, storage);
}

acm::Buffer acm::Device::createBuffer(size_t size, acm::BufferUsage usage)
{
	return acm::Buffer(*this, size, usage);
}

acm::Sampler acm::Device::createSampler(float maxAnisotropy)
{
	return acm::Sampler(*this, maxAnisotropy);
}

acm::DescriptorSetLayout acm::Device::createDescriptorSetLayout(uint32_t samplerCount)
{
	// Convenience: N fragment-stage combined-image-samplers at bindings 0..n-1.
	std::vector<acm::DescriptorBinding> bindings(samplerCount);
	for (uint32_t i = 0; i < samplerCount; ++i)
		bindings[i] = {i, acm::DescriptorType::CombinedImageSampler, acm::ShaderStage::Fragment};
	return acm::DescriptorSetLayout(*this, bindings);
}

acm::DescriptorSetLayout acm::Device::createDescriptorSetLayout(const std::vector<acm::DescriptorBinding>& bindings)
{
	return acm::DescriptorSetLayout(*this, bindings);
}

acm::DescriptorSet acm::Device::createDescriptorSet(acm::DescriptorSetLayout layout)
{
	return acm::DescriptorSet(*this, layout);
}

acm::UniformRing acm::Device::createUniformRing(size_t bytes, uint32_t binding, acm::ShaderStage stage, uint32_t frames)
{
	return acm::UniformRing(*this, bytes, binding, stage, frames);
}

acm::Shader acm::Device::createShader(const std::vector<char>& spirv)
{
	return acm::Shader(*this, spirv);
}

acm::Pipeline acm::Device::createPipeline(acm::Shader vertex, acm::Shader fragment, VkRenderPass renderPass)
{
	acm::PipelineConfig config;
	config.vertex = vertex;
	config.fragment = fragment;
	config.renderPass = renderPass;
	return acm::Pipeline(*this, config);
}

acm::Pipeline acm::Device::createPipeline(const acm::PipelineConfig& config)
{
	return acm::Pipeline(*this, config);
}

acm::ComputePipeline acm::Device::createComputePipeline(acm::Shader compute, acm::DescriptorSetLayout layout)
{
	return acm::ComputePipeline(*this, compute, layout);
}

acm::CommandPool acm::Device::createCommandPool()
{
	return acm::CommandPool(*this);
}

acm::Renderer acm::Device::createRenderer(acm::SwapChain swapChain)
{
	return acm::Renderer(*this, swapChain);
}

const acm::GPU& acm::Device::getGPU() const
{
	return m->gpu;
}

uint32_t acm::Device::getQueueIdx() const
{
	return m->queueIdx;
}

const acm::GPUFeatures& acm::Device::enabledFeatures() const
{
	return m->enabledFeatures;
}

acm::SampleCount acm::Device::maxSampleCount() const
{
	return acm::maxSampleCount(m->gpu.device);
}

size_t acm::Device::minUniformBufferOffsetAlignment() const
{
	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(m->gpu.device, &props);
	return size_t(props.limits.minUniformBufferOffsetAlignment);
}

acm::MemoryAllocator& acm::Device::memoryAllocator()
{
	return *m->allocator;
}

size_t acm::Device::memoryBlockCount() const
{
	return m->allocator->blockCount();
}

VkDevice acm::Device::vkDevice()
{
	return m->device;
}

VkQueue acm::Device::vkQueue()
{
	return m->queue;
}

void acm::Device::waitIdle()
{
	vkDeviceWaitIdle(m->device);
}

acm::Error acm::Device::submitSync(const std::function<void(acm::CommandBuffer)>& record)
{
	acm::CommandPool pool = createCommandPool();
	acm::CommandBuffer cmd = pool.allocate();
	if (!cmd.valid())
		return acm::Error("submitSync: failed to allocate command buffer");

	if (auto err = cmd.begin())
		return err;
	record(cmd);
	if (auto err = cmd.end())
		return err;

	VkCommandBuffer vkcb = cmd.vkCommandBuffer();
	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &vkcb;

	// Queue access is externally synchronized — guard submit + wait like the renderer does.
	std::lock_guard<std::mutex> lock(m->deviceMutex);
	if (vkQueueSubmit(m->queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS)
		return acm::Error("submitSync: failed to submit");
	vkQueueWaitIdle(m->queue);
	return acm::Error{};
	// pool + cmd are acm handles; their teardown defers onto the device's graveyard.
}

std::mutex& acm::Device::deviceMutex()
{
	return m->deviceMutex;
}

void acm::Device::beginFrame()
{
	++m->currentFrame;
}

uint64_t acm::Device::currentFrame() const
{
	return m->currentFrame;
}

void acm::Device::enqueueDestroy(std::function<void()> destroy)
{
	m->graveyard.push_back({m->currentFrame, std::move(destroy)});
}

void acm::Device::collectGarbage(uint64_t completedFrame)
{
	auto& graveyard = m->graveyard;
	auto it = graveyard.begin();
	while (it != graveyard.end())
	{
		if (it->frame <= completedFrame)
		{
			it->destroy();
			it = graveyard.erase(it);
		}
		else
		{
			++it;
		}
	}
}