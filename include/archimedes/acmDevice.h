#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmGPU.h"
#include "archimedes/acmNative.h"
#include "archimedes/acmTypes.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace acm
{
	class Device
	{
	public:
		Device();
		Device(const acm::Device& other) = delete;
		Device& operator=(const acm::Device& other) = delete;
		Device(acm::Device&& other) noexcept;
		Device& operator=(acm::Device&& other) noexcept;
		~Device();

		void reset();
		bool valid() const;
		acm::Error error() const;

		// Factories — the only way to build children of a Device.
		acm::SwapChain createSwapChain(const acm::Surface& surface, acm::SurfaceFormat format, acm::PresentMode presentMode, acm::Extent2D desiredExtent = {}, bool depth = false, acm::SampleCount samples = acm::SampleCount::One);
		acm::RenderTarget createRenderTarget(const acm::Texture& texture, acm::RenderTargetFinish finish = acm::RenderTargetFinish::Sampled, bool depth = false, acm::SampleCount samples = acm::SampleCount::One); // offscreen: owns its render pass
		acm::Shader createShader(const std::vector<char>& spirv);
		// Convenience: a pipeline with no vertex input and no descriptors (geometry
		// from the shader). For vertex buffers / descriptors, use the config form.
		acm::Pipeline createPipeline(const acm::Shader& vertex, const acm::Shader& fragment, const acm::RenderTarget& target);
		acm::Pipeline createPipeline(const acm::PipelineConfig& config);
		// A compute pipeline: a compute `Shader` + a `DescriptorSetLayout` for the
		// resources it reads/writes (a storage buffer, an optional uniform). Record
		// bindComputePipeline -> bindComputeDescriptorSet -> dispatch, then submit.
		acm::ComputePipeline createComputePipeline(const acm::Shader& compute, const acm::DescriptorSetLayout& layout);
		acm::CommandPool createCommandPool();
		acm::Renderer createRenderer(const acm::SwapChain& swapChain);
		// `mipmapped` (color only) gives the texture a full mip chain that Texture::upload
		// generates; the result is a sampling resource, not a RenderTarget attachment.
		// `storage` (color only) adds STORAGE usage so a compute shader can write it via a
		// StorageImage descriptor.
		acm::Texture createTexture(acm::Format format, acm::Extent2D extent, bool mipmapped = false, bool storage = false);
		acm::Buffer createBuffer(size_t size, acm::BufferUsage usage);
		// maxAnisotropy > 1 enables anisotropic filtering (needs the samplerAnisotropy
		// feature; clamped to the device limit, disabled with a warning if unsupported).
		acm::Sampler createSampler(float maxAnisotropy = 1.0f);
		// Convenience: N fragment-stage combined-image-samplers (bindings 0..n-1).
		acm::DescriptorSetLayout createDescriptorSetLayout(uint32_t samplerCount);
		acm::DescriptorSetLayout createDescriptorSetLayout(const std::vector<acm::DescriptorBinding>& bindings);
		acm::DescriptorSet createDescriptorSet(const acm::DescriptorSetLayout& layout);

		const acm::GPU& getGPU() const;
		uint32_t getQueueIdx() const;
		// The curated optional features actually enabled on this device (the subset of
		// the GPU's that the renderer turned on). Check before using, e.g., wireframe.
		const acm::GPUFeatures& enabledFeatures() const;
		// The highest MSAA sample count usable for color+depth framebuffers here. A
		// SampleCount request beyond this is clamped down to it.
		acm::SampleCount maxSampleCount() const;
		// Required alignment (bytes) for a dynamic uniform buffer offset — size each
		// per-object slice packed into one buffer to a multiple of this.
		size_t minUniformBufferOffsetAlignment() const;

		size_t memoryBlockCount() const; // live native memory blocks in the pool

		// Blocks until the device is idle (all queues drained). Like the queue, host
		// access must be externally synchronized — call it only when no other thread
		// is submitting (e.g. the testbed's fork-join calls it on the main thread while
		// both render threads are parked, to fence a shared-resource update against
		// in-flight reads).
		void waitIdle();

		// Records `record` into a transient command buffer, submits it to the queue, and
		// waits the queue idle before returning — synchronous one-shot GPU work (a compute
		// dispatch, a transfer). It stalls the queue, so it is a load-time / per-frame-at-
		// most tool, not a hot path, and is externally synchronized like waitIdle (locks
		// the device mutex around submit + wait; don't call it while another thread
		// submits). The callback records into an already-begun command buffer.
		acm::Error submitSync(const std::function<void(acm::CommandBuffer& cmd)>& record);
		acm::Error submitSync(const acm::CommandBuffer& commandBuffer);

	private:
		friend acm::native::Instance; // only Instance::createDevice builds one
		explicit Device(std::unique_ptr<acm::native::Device> device);

		std::unique_ptr<acm::native::Device> m;
		acm::Error m_error;
	};
} // namespace acm
