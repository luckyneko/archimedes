#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmGPU.h"
#include "archimedes/acmHandle.h"
#include "archimedes/acmTypes.h"
#include "archimedes/HandleMap.h"
#include "archimedes/vulkan/Memory.h"
#include "archimedes/vulkan/Resources.h"

#include <vulkan/vulkan.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace acm::vulkan
{
	class Device
	{
	public:
		Device(acm::vulkan::Instance& instance, const acm::GPU& gpu, uint32_t queueIdx);
		~Device();

		bool valid() const { return m_device != VK_NULL_HANDLE; }
		acm::Error error() const { return m_error; }

		const acm::GPU& gpu() const { return m_gpu; }
		acm::vulkan::Instance& instance() const { return *m_instance; }
		uint32_t queueIndex() const { return m_queueIndex; }
		const acm::GPUFeatures& enabledFeatures() const { return m_enabledFeatures; }
		acm::SampleCount maxSampleCount() const;
		size_t minUniformBufferOffsetAlignment() const;

		size_t memoryBlockCount() const { return m_allocator->blockCount(); }

		void waitIdle();
		void beginFrame();
		uint64_t currentFrame() const { return m_currentFrame.load(std::memory_order_relaxed); }
		void enqueueDestroy(std::function<void()> destroy);
		void collectGarbage(uint64_t completedFrame);
		VkDevice vkDevice() const { return m_device; }
		VkPhysicalDevice vkPhysicalDevice() const { return m_physicalDevice; }
		acm::vulkan::MemoryAllocator& allocator() const { return *m_allocator; }
		acm::Error copyBuffer(VkBuffer source, VkBuffer destination, VkDeviceSize size);
		acm::Error submitOneShot(const std::function<void(VkCommandBuffer)>& record);
		acm::Error submitFrame(VkCommandBuffer commandBuffer, VkSemaphore imageAvailable, VkSemaphore renderFinished, VkFence inFlight, VkSwapchainKHR swapChain, uint32_t imageIndex, bool& needsRecreate);

		acm::Buffer createBuffer(size_t size, acm::BufferUsage usage);

		acm::Texture createTexture(acm::Format format, acm::Extent2D extent, bool mipmapped, bool storage);

		acm::Sampler createSampler(float maxAnisotropy);

		acm::Shader createShader(const std::vector<char>& spirv);

		acm::DescriptorSetLayout createDescriptorSetLayout(const std::vector<acm::DescriptorBinding>& bindings);

		acm::DescriptorSet createDescriptorSet(const acm::DescriptorSetLayout& layout);

		acm::Pipeline createPipeline(const acm::PipelineConfig& config);

		acm::ComputePipeline createComputePipeline(const acm::Shader& compute, const acm::DescriptorSetLayout& layout);

		acm::RenderTarget createRenderTarget(VkRenderPass renderPass, VkImage image, acm::Format format, acm::Extent2D extent, bool depth, acm::SampleCount samples);
		acm::RenderTarget createRenderTarget(const acm::Texture& texture, acm::RenderTargetFinish finish, bool depth, acm::SampleCount samples);

		acm::SwapChain createSwapChain(const acm::Surface& surface, acm::SurfaceFormat format, acm::PresentMode presentMode, acm::Extent2D desiredExtent, bool depth, acm::SampleCount samples);

		acm::CommandPool createCommandPool();
		acm::CommandBuffer allocateCommandBuffer(acm::vulkan::CommandPool* poolResource, const acm::Handle& pool);
		acm::Error submitCommandBufferSync(const acm::CommandBuffer& commandBuffer);

		acm::Renderer createRenderer(const acm::SwapChain& swapChain);

	private:
		struct Pending
		{
			uint64_t frame{0};
			std::function<void()> destroy;
		};

		acm::vulkan::Instance* m_instance{nullptr};
		acm::GPU m_gpu;
		uint32_t m_queueIndex{0};
		acm::GPUFeatures m_enabledFeatures;
		VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
		VkDevice m_device{VK_NULL_HANDLE};
		VkQueue m_queue{VK_NULL_HANDLE};
		std::unique_ptr<acm::vulkan::MemoryAllocator> m_allocator;
		std::mutex m_queueMutex;
		std::mutex m_graveyardMutex;
		std::atomic<uint64_t> m_currentFrame{0};
		std::vector<Pending> m_graveyard;
		acm::HandleMap<acm::vulkan::Buffer, Device> m_buffers;
		acm::HandleMap<acm::vulkan::Texture, Device> m_textures;
		acm::HandleMap<acm::vulkan::Sampler, Device> m_samplers;
		acm::HandleMap<acm::vulkan::Shader, Device> m_shaders;
		acm::HandleMap<acm::vulkan::DescriptorSetLayout, Device> m_descriptorSetLayouts;
		acm::HandleMap<acm::vulkan::DescriptorSet, Device> m_descriptorSets;
		acm::HandleMap<acm::vulkan::Pipeline, Device> m_pipelines;
		acm::HandleMap<acm::vulkan::ComputePipeline, Device> m_computePipelines;
		acm::HandleMap<acm::vulkan::RenderTarget, Device> m_renderTargets;
		acm::HandleMap<acm::vulkan::SwapChain, Device> m_swapChains;
		acm::HandleMap<acm::vulkan::CommandPool, Device> m_commandPools;
		acm::HandleMap<acm::vulkan::CommandBuffer, Device> m_commandBuffers;
		acm::HandleMap<acm::vulkan::Renderer, Device> m_renderers;
		acm::Error m_error;
	};
} // namespace acm::vulkan
