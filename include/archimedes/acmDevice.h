/*
 *  Created by LuckyNeko on 07/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmBackend.h"
#include "archimedes/acmDeviceInfo.h"
#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmTypes.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace acm
{
	// Unique owning root for device resources. Factories allocate children in stable
	// typed resource pools; every child wrapper must be reset before the Device.
	// An invalid Device (default-constructed, moved-from, reset, or failed creation) is
	// safe to call: factories return invalid children, capability queries return neutral
	// values (empty DeviceInfo, no features, SampleCount::One, alignment 1, no memory
	// blocks), waitIdle() does nothing, and submitSync returns an error.
	class Device
	{
	public:
		// Lifetime
		Device();
		Device(const acm::Device& other) = delete;
		Device& operator=(const acm::Device& other) = delete;
		Device(acm::Device&& other) noexcept;
		Device& operator=(acm::Device&& other) noexcept;
		~Device();

		// State
		void reset();
		bool valid() const;
		acm::Error error() const;
		// Opaque backend handle for the Vulkan-interop seam (acmVulkanInterop.h). Not a
		// public native-handle API on its own — the raw VkPhysicalDevice/VkDevice/VkQueue
		// accessors live in acm::interop.
		acm::backend::Device* backend() const;

		// Factories
		// The only public way to build Device children.
		acm::SwapChain createSwapChain(const acm::Surface& surface, const acm::SurfaceOption& option, const acm::SwapChainConfig& config = {});
		acm::SwapChain createSwapChain(const acm::Surface& surface, acm::SurfaceFormat format, acm::PresentMode presentMode, const acm::SwapChainConfig& config = {});
		acm::RenderTarget createRenderTarget(const acm::Texture& texture, const acm::RenderTargetConfig& config = {});
		// spirv is compiled SPIR-V bytecode; loading it from disk is the caller's job.
		acm::Shader createShader(const std::vector<char>& spirv);
		acm::Pipeline createPipeline(const acm::PipelineShaders& shaders, const acm::RenderTarget& target);
		acm::Pipeline createPipeline(const acm::PipelineShaders& shaders, const acm::RenderTarget& target, const acm::PipelineConfig& config);
		// A compute pipeline: a compute `Shader` + a `DescriptorSetLayout` for the
		// resources it reads/writes (a storage buffer, an optional uniform). Record
		// bindComputePipeline -> bindComputeDescriptorSet -> dispatch, then submit.
		acm::ComputePipeline createComputePipeline(const acm::Shader& compute, const acm::DescriptorSetLayout& layout);
		acm::CommandPool createCommandPool();
		acm::Renderer createRenderer(const acm::SwapChain& swapChain);
		// TextureConfig::mipmapped (color only) gives the texture a full mip chain that Texture::upload
		// generates; the result is a sampling resource, not a RenderTarget attachment.
		// TextureConfig::storage (color only) adds STORAGE usage so a compute shader can write it via a
		// StorageImage descriptor.
		acm::Texture createTexture(acm::Format format, acm::Extent2D extent, const acm::TextureConfig& config = {});
		acm::Buffer createBuffer(size_t size, acm::BufferUsage usage);
		// maxAnisotropy > 1 enables anisotropic filtering (needs the samplerAnisotropy
		// feature; clamped to the device limit; without the feature the sampler is
		// isotropic and a warning is logged through the log hook, acmLog.h).
		acm::Sampler createSampler(float maxAnisotropy = 1.0f);
		// Convenience: N fragment-stage combined-image-samplers (bindings 0..n-1).
		acm::DescriptorSetLayout createDescriptorSetLayout(uint32_t samplerCount);
		acm::DescriptorSetLayout createDescriptorSetLayout(const std::vector<acm::DescriptorBinding>& bindings);
		acm::DescriptorSet createDescriptorSet(const acm::DescriptorSetLayout& layout);

		// Capabilities
		const acm::DeviceInfo& deviceInfo() const;
		uint32_t queueFamily() const;
		// The curated optional features actually enabled on this device. Check before
		// using, e.g., wireframe.
		const acm::DeviceFeatures& enabledFeatures() const;
		// The highest MSAA sample count usable for color+depth rendering targets here. A
		// SampleCount the device does not support is clamped down to the highest supported
		// count below it, and the render target or swapchain asking for it logs a warning.
		acm::SampleCount maxSampleCount() const;
		// Required alignment (bytes) for a dynamic uniform buffer offset — size each
		// per-object slice packed into one buffer to a multiple of this.
		size_t minUniformBufferOffsetAlignment() const;

		size_t memoryBlockCount() const; // live native memory blocks in the pool

		// Synchronization
		// Blocks until the device is idle (all queues drained), then collects garbage:
		// resources whose last wrapper was dropped are destroyed once the GPU finished the
		// submissions made before the wait. Safe to call while other threads submit (a
		// Renderer::render on another thread, another submitSync): it holds the device's
		// queue mutex for the wait, so those threads stall until it returns. Use it to fence
		// a shared-resource update against in-flight reads. Never call it from inside
		// acm::interop::withQueue, which already holds that mutex: it would deadlock.
		void waitIdle();

		// Records `record` into a transient command buffer, submits it to the queue, and
		// waits the queue idle before returning — synchronous one-shot GPU work (a compute
		// dispatch, a transfer) — then collects garbage like waitIdle. It stalls the queue,
		// so it is a load-time / per-frame-at-most tool, not a hot path. Safe to call while
		// other threads submit: the queue mutex is held around submit + wait, so they stall
		// until it returns. Never call it from inside acm::interop::withQueue (deadlock).
		// The callback records into an already-begun command buffer. If any of its
		// recording calls was rejected, nothing is submitted and the first rejection
		// (CommandBuffer::error()) is returned. Resources the callback records must stay
		// alive until submitSync returns (see acmCommandBuffer.h, Lifetime).
		acm::Error submitSync(const std::function<void(acm::CommandBuffer& cmd)>& record);
		// Submits an ended command buffer as recorded, with the same waiting, locking and
		// garbage collection as above; a rejected recording call is not in it, so check
		// commandBuffer.error() first when that matters.
		acm::Error submitSync(const acm::CommandBuffer& commandBuffer);

	private:
		// Construction
		friend acm::backend::Instance;
		explicit Device(std::unique_ptr<acm::backend::Device> device);

		std::unique_ptr<acm::backend::Device> m;
		acm::Error m_error;
	};
} // namespace acm
