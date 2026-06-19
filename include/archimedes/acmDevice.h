#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmGPU.h"
#include "archimedes/acmRenderer.h" // Renderer::MaxFramesInFlight (default ring depth)
#include "archimedes/acmTypes.h"
#include "archimedes/acmVkFwd.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

namespace acm
{
	namespace detail
	{
		class MemoryAllocator; // internal pooling sub-allocator (src/acmVkMemory.h)
	}

	class Device
	{
	public:
		Device() {}

		inline void reset() { m.reset(); m_error = {}; }
		inline bool valid() const { return m != nullptr; }
		acm::Error error() const { return m_error; }

		// Factories — the only way to build children of a Device.
		acm::SwapChain createSwapChain(acm::Surface surface, acm::SurfaceFormat format, acm::PresentMode presentMode, acm::Extent2D desiredExtent = {}, bool depth = false, acm::SampleCount samples = acm::SampleCount::One);
		acm::RenderTarget createRenderTarget(VkRenderPass renderPass, VkImage image, acm::Format format, acm::Extent2D extent, bool depth = false, acm::SampleCount samples = acm::SampleCount::One);
		acm::RenderTarget createRenderTarget(acm::Texture texture, acm::RenderTargetFinish finish = acm::RenderTargetFinish::Sampled, bool depth = false, acm::SampleCount samples = acm::SampleCount::One); // offscreen: owns its render pass
		acm::Shader createShader(const std::vector<char>& spirv);
		// Convenience: a pipeline with no vertex input and no descriptors (geometry
		// from the shader). For vertex buffers / descriptors, use the config form.
		acm::Pipeline createPipeline(acm::Shader vertex, acm::Shader fragment, VkRenderPass renderPass);
		acm::Pipeline createPipeline(const acm::PipelineConfig& config);
		// A compute pipeline: a compute `Shader` + a `DescriptorSetLayout` for the
		// resources it reads/writes (a storage buffer, an optional uniform). Record
		// bindComputePipeline -> bindComputeDescriptorSet -> dispatch, then submit.
		acm::ComputePipeline createComputePipeline(acm::Shader compute, acm::DescriptorSetLayout layout);
		acm::CommandPool createCommandPool();
		acm::Renderer createRenderer(acm::SwapChain swapChain);
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
		acm::DescriptorSet createDescriptorSet(acm::DescriptorSetLayout layout);
		// A per-frame ring of `frames` uniform buffers + one-binding descriptor sets,
		// for a uniform updated every frame (e.g. an MVP matrix). Defaults: binding 0,
		// vertex stage, MaxFramesInFlight deep.
		acm::UniformRing createUniformRing(size_t bytes, uint32_t binding = 0, acm::ShaderStage stage = acm::ShaderStage::Vertex, uint32_t frames = acm::Renderer::MaxFramesInFlight);

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

		// The device's pooling memory sub-allocator — Texture/Buffer allocate from it
		// rather than calling vkAllocateMemory per resource. Internal; exposed for those
		// resource types (and tests via memoryBlockCount).
		acm::detail::MemoryAllocator& memoryAllocator();
		size_t memoryBlockCount() const; // live VkDeviceMemory blocks in the pool
		VkDevice vkDevice();
		VkQueue vkQueue();

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
		acm::Error submitSync(const std::function<void(acm::CommandBuffer cmd)>& record);

		// Guards the single VkQueue (submit/present) and the device-shared frame /
		// graveyard bookkeeping below, so several Renderers can drive their own
		// swapchains from their own threads. The Renderer locks this around
		// submit/present + recreate + beginFrame/collectGarbage; command recording runs
		// unlocked (concurrent). Single-window use just locks an uncontended mutex. The
		// lock is the caller's responsibility (the bookkeeping methods below stay
		// lock-free, so they don't deadlock when called under it).
		std::mutex& deviceMutex();

		// Deferred destruction. A Vulkan object must outlive every GPU
		// submission that references it, which the CPU-side handle refcount
		// cannot know about. Resources therefore enqueue their teardown here
		// (tagged with the current frame) instead of destroying inline; the
		// render loop calls beginFrame() once per frame and collectGarbage()
		// with the newest frame index known to have fully retired on the GPU.
		// Lock-free by design: a multi-threaded caller holds deviceMutex() around
		// these (see above), so they must not lock it themselves. The Device
		// destructor waits idle then flushes everything still pending.
		void beginFrame();
		uint64_t currentFrame() const;
		void enqueueDestroy(std::function<void()> destroy);
		void collectGarbage(uint64_t completedFrame);

	private:
		friend class Instance; // only Instance::createDevice builds one
		Device(acm::Instance instance, const acm::GPU& gpu, uint32_t queueIdx);

		struct impl;
		std::shared_ptr<impl> m;
		acm::Error m_error;
	};
} // namespace acm
