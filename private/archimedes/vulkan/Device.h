/*
 *  Created by LuckyNeko on 24/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmGPU.h"
#include "archimedes/acmTypes.h"
#include "archimedes/DeferredDestroyQueue.h"
#include "archimedes/ResourcePool.h"
#include "archimedes/vulkan/Memory.h"
#include "archimedes/vulkan/Resources.h"

#include <vulkan/vulkan.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace acm::vulkan
{
	// Move-only logical-device backend. Owns every device-scoped resource pool,
	// the queue submission path, deferred destruction, and the memory allocator.
	class Device
	{
	public:
		// Lifetime
		Device(acm::vulkan::Instance& instance, const acm::DeviceInfo& deviceInfo, uint32_t queueIndex);
		~Device();

		// State
		bool valid() const { return m_device != VK_NULL_HANDLE; }
		acm::Error error() const { return m_error; }

		// Capabilities
		const acm::DeviceInfo& deviceInfo() const { return m_deviceInfo; }
		acm::vulkan::Instance& instance() const { return *m_instance; }
		uint32_t queueIndex() const { return m_queueIndex; }
		const acm::DeviceFeatures& enabledFeatures() const { return m_enabledFeatures; }
		const VkPhysicalDeviceProperties& properties() const { return m_properties.properties; }
		VkSampleCountFlagBits sampleCount(acm::SampleCount requested) const;
		acm::SampleCount maxSampleCount() const;
		size_t minUniformBufferOffsetAlignment() const;

		size_t memoryBlockCount() const { return m_allocator->blockCount(); }

		// Synchronization
		void waitIdle();
		void collectGarbage(uint64_t completedSerial);

		// Native access
		VkDevice vkDevice() const { return m_device; }
		VkPhysicalDevice vkPhysicalDevice() const { return m_physicalDevice; }
		acm::vulkan::MemoryAllocator& allocator() const { return *m_allocator; }

		// Submission
		acm::Error copyBuffer(VkBuffer source, VkBuffer destination, VkDeviceSize size);
		acm::Error submitOneShot(const std::function<void(VkCommandBuffer)>& record);
		acm::Error submitFrame(VkCommandBuffer commandBuffer, VkSemaphore imageAvailable, VkSemaphore renderFinished, VkFence inFlight, VkSwapchainKHR swapChain, uint32_t imageIndex, bool& needsRecreate, uint64_t& submittedSerial);

		// Factories
		acm::Buffer createBuffer(size_t size, acm::BufferUsage usage);
		acm::Texture createTexture(acm::Format format, acm::Extent2D extent, bool mipmapped, bool storage);
		acm::Sampler createSampler(float maxAnisotropy);
		acm::Shader createShader(const std::vector<char>& spirv);
		acm::DescriptorSetLayout createDescriptorSetLayout(const std::vector<acm::DescriptorBinding>& bindings);
		acm::DescriptorSet createDescriptorSet(const acm::DescriptorSetLayout& layout);
		acm::Pipeline createPipeline(const acm::PipelineConfig& config);
		acm::ComputePipeline createComputePipeline(const acm::Shader& compute, const acm::DescriptorSetLayout& layout);
		acm::RenderTarget createRenderTarget(VkImage image, acm::Format format, acm::Extent2D extent, bool depth, acm::SampleCount samples);
		acm::RenderTarget createRenderTarget(const acm::Texture& texture, acm::RenderTargetFinish finish, bool depth, acm::SampleCount samples);
		bool invalidateRenderTarget(acm::RenderTarget& target);
		acm::SwapChain createSwapChain(const acm::Surface& surface, acm::SurfaceFormat format, acm::PresentMode presentMode, acm::Extent2D desiredExtent, bool depth, acm::SampleCount samples);
		acm::CommandPool createCommandPool();
		acm::CommandBuffer allocateCommandBuffer(const acm::CommandPool& pool);
		acm::Error submitCommandBufferSync(const acm::CommandBuffer& commandBuffer);
		acm::Renderer createRenderer(const acm::SwapChain& swapChain);

	private:
		// Internals
		static acm::Error constructionError(acm::Error error, const char* fallback);
		VkResult queueSubmit(VkCommandBuffer commandBuffer, const VkSemaphoreSubmitInfo* waitSemaphore, const VkSemaphoreSubmitInfo* signalSemaphore, VkFence fence, uint64_t& submittedSerial);
		template <typename T, typename Constructor>
		auto emplaceResource(acm::ResourcePool<T>& pool, Constructor&& construct)
		{
			return pool.emplace(std::forward<Constructor>(construct));
		}
		template <typename T>
		void clearResourcePool(acm::ResourcePool<T>& pool)
		{
			pool.clear([this](T&& resource)
					   { m_deferredDestroy.enqueue(m_lastSubmittedSerial.load(std::memory_order_relaxed), std::move(resource)); });
		}
		template <typename T>
		void collectResourcePool(acm::ResourcePool<T>& pool)
		{
			pool.collectGarbage([this](T&& resource)
								{ m_deferredDestroy.enqueue(m_lastSubmittedSerial.load(std::memory_order_relaxed), std::move(resource)); });
		}

		acm::vulkan::Instance* m_instance{nullptr};
		acm::DeviceInfo m_deviceInfo;
		uint32_t m_queueIndex{0};
		acm::DeviceFeatures m_enabledFeatures;
		VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
		VkPhysicalDeviceProperties2 m_properties{};
		VkDevice m_device{VK_NULL_HANDLE};
		VkQueue m_queue{VK_NULL_HANDLE};
		std::unique_ptr<acm::vulkan::MemoryAllocator> m_allocator;
		std::mutex m_queueMutex;
		std::atomic<uint64_t> m_lastSubmittedSerial{0};
		acm::DeferredDestroyQueue m_deferredDestroy;
		acm::ResourcePool<acm::vulkan::Buffer> m_buffers;
		acm::ResourcePool<acm::vulkan::Texture> m_textures;
		acm::ResourcePool<acm::vulkan::Sampler> m_samplers;
		acm::ResourcePool<acm::vulkan::Shader> m_shaders;
		acm::ResourcePool<acm::vulkan::DescriptorSetLayout> m_descriptorSetLayouts;
		acm::ResourcePool<acm::vulkan::DescriptorSet> m_descriptorSets;
		acm::ResourcePool<acm::vulkan::Pipeline> m_pipelines;
		acm::ResourcePool<acm::vulkan::ComputePipeline> m_computePipelines;
		acm::ResourcePool<acm::vulkan::RenderTarget> m_renderTargets;
		acm::ResourcePool<acm::vulkan::SwapChain> m_swapChains;
		acm::ResourcePool<acm::vulkan::CommandPool> m_commandPools;
		acm::ResourcePool<acm::vulkan::CommandBuffer> m_commandBuffers;
		acm::ResourcePool<acm::vulkan::Renderer> m_renderers;
		acm::Error m_error;
	};
} // namespace acm::vulkan
