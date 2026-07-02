/*
 *  Created by LuckyNeko on 07/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

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
#include "archimedes/nativeAPI.h"

#include <utility>

namespace acm
{
	// -----------------------------------------------------------------------------
	// Lifetime
	// -----------------------------------------------------------------------------

	Device::Device() = default;

	Device::Device(Device&& other) noexcept = default;

	Device& Device::operator=(Device&& other) noexcept = default;

	Device::~Device() = default;

	// -----------------------------------------------------------------------------
	// State
	// -----------------------------------------------------------------------------

	void Device::reset()
	{
		m.reset();
		m_error = {};
	}

	bool Device::valid() const
	{
		return m != nullptr;
	}

	Error Device::error() const
	{
		return m_error;
	}

	// -----------------------------------------------------------------------------
	// Factories
	// -----------------------------------------------------------------------------

	SwapChain Device::createSwapChain(const Surface& surface, SurfaceFormat format, PresentMode presentMode, Extent2D desiredExtent, bool depth, SampleCount samples)
	{
		return m ? m->createSwapChain(surface, format, presentMode, desiredExtent, depth, samples) : SwapChain{};
	}

	RenderTarget Device::createRenderTarget(const Texture& texture, RenderTargetFinish finish, bool depth, SampleCount samples)
	{
		return m ? m->createRenderTarget(texture, finish, depth, samples) : RenderTarget{};
	}

	Shader Device::createShader(const std::vector<char>& spirv)
	{
		return m ? m->createShader(spirv) : Shader{};
	}

	Pipeline Device::createPipeline(const Shader& vertex, const Shader& fragment, const RenderTarget& target)
	{
		PipelineConfig config;
		config.vertex = vertex;
		config.fragment = fragment;
		config.target = target;
		return m ? m->createPipeline(config) : Pipeline{};
	}

	Pipeline Device::createPipeline(const PipelineConfig& config)
	{
		return m ? m->createPipeline(config) : Pipeline{};
	}

	ComputePipeline Device::createComputePipeline(const Shader& compute, const DescriptorSetLayout& layout)
	{
		return m ? m->createComputePipeline(compute, layout) : ComputePipeline{};
	}

	CommandPool Device::createCommandPool()
	{
		return m ? m->createCommandPool() : CommandPool{};
	}

	Renderer Device::createRenderer(const SwapChain& swapChain)
	{
		return m ? m->createRenderer(swapChain) : Renderer{};
	}

	Texture Device::createTexture(Format format, Extent2D extent, bool mipmapped, bool storage)
	{
		return m ? m->createTexture(format, extent, mipmapped, storage) : Texture{};
	}

	Buffer Device::createBuffer(size_t size, BufferUsage usage)
	{
		return m ? m->createBuffer(size, usage) : Buffer{};
	}

	Sampler Device::createSampler(float maxAnisotropy)
	{
		return m ? m->createSampler(maxAnisotropy) : Sampler{};
	}

	DescriptorSetLayout Device::createDescriptorSetLayout(uint32_t samplerCount)
	{
		std::vector<DescriptorBinding> bindings(samplerCount);
		for (uint32_t i = 0; i < samplerCount; ++i)
			bindings[i] = {i, DescriptorType::CombinedImageSampler, ShaderStage::Fragment};
		return m ? m->createDescriptorSetLayout(bindings) : DescriptorSetLayout{};
	}

	DescriptorSetLayout Device::createDescriptorSetLayout(const std::vector<DescriptorBinding>& bindings)
	{
		return m ? m->createDescriptorSetLayout(bindings) : DescriptorSetLayout{};
	}

	DescriptorSet Device::createDescriptorSet(const DescriptorSetLayout& layout)
	{
		return m ? m->createDescriptorSet(layout) : DescriptorSet{};
	}

	// -----------------------------------------------------------------------------
	// Capabilities
	// -----------------------------------------------------------------------------

	const GPU& Device::getGPU() const
	{
		return m->gpu();
	}

	uint32_t Device::getQueueIdx() const
	{
		return m->queueIndex();
	}

	const GPUFeatures& Device::enabledFeatures() const
	{
		return m->enabledFeatures();
	}

	SampleCount Device::maxSampleCount() const
	{
		return m->maxSampleCount();
	}

	size_t Device::minUniformBufferOffsetAlignment() const
	{
		return m->minUniformBufferOffsetAlignment();
	}

	size_t Device::memoryBlockCount() const
	{
		return m->memoryBlockCount();
	}

	// -----------------------------------------------------------------------------
	// Synchronization
	// -----------------------------------------------------------------------------

	void Device::waitIdle()
	{
		m->waitIdle();
	}

	Error Device::submitSync(const std::function<void(CommandBuffer&)>& record)
	{
		CommandPool pool = createCommandPool();
		CommandBuffer commandBuffer = pool.allocate();
		if (!commandBuffer.valid())
			return Error("submitSync: failed to allocate command buffer");

		if (auto error = commandBuffer.begin())
			return error;
		record(commandBuffer);
		if (auto error = commandBuffer.end())
			return error;

		return submitSync(commandBuffer);
	}

	Error Device::submitSync(const CommandBuffer& commandBuffer)
	{
		return m ? m->submitCommandBufferSync(commandBuffer) : Error("submitSync: invalid device");
	}

	// -----------------------------------------------------------------------------
	// Construction
	// -----------------------------------------------------------------------------

	Device::Device(std::unique_ptr<native::Device> device)
	{
		if (!device->valid())
		{
			m_error = device->error();
			return;
		}
		m = std::move(device);
	}

} // namespace acm
