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
#include "archimedes/backendAPI.h"

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

	SwapChain Device::createSwapChain(const Surface& surface, const SurfaceOption& option, const SwapChainConfig& config)
	{
		if (!m || option.device.deviceIndex != deviceInfo().index)
			return SwapChain{};
		backend::Surface* backendSurface = surface.backend();
		if (!backendSurface)
			return SwapChain{};

		const SurfaceDeviceSupport* support = nullptr;
		for (const SurfaceDeviceSupport& candidate : backendSurface->support())
			if (candidate.deviceIndex == option.device.deviceIndex)
			{
				support = &candidate;
				break;
			}
		if (!support || option.device.queueFamily >= support->queuePresentSupport.size() || !support->queuePresentSupport[option.device.queueFamily])
			return SwapChain{};

		bool formatSupported = false;
		for (const SurfaceFormat& format : support->formats)
			if (format.format == option.format.format && format.colorSpace == option.format.colorSpace)
			{
				formatSupported = true;
				break;
			}
		if (!formatSupported)
			return SwapChain{};

		bool presentModeSupported = false;
		for (PresentMode presentMode : support->presentModes)
			if (presentMode == option.presentMode)
			{
				presentModeSupported = true;
				break;
			}
		if (!presentModeSupported)
			return SwapChain{};

		return createSwapChain(surface, option.format, option.presentMode, config);
	}

	SwapChain Device::createSwapChain(const Surface& surface, SurfaceFormat format, PresentMode presentMode, const SwapChainConfig& config)
	{
		return m ? m->createSwapChain(surface, format, presentMode, config) : SwapChain{};
	}

	RenderTarget Device::createRenderTarget(const Texture& texture, const RenderTargetConfig& config)
	{
		return m ? m->createRenderTarget(texture, config) : RenderTarget{};
	}

	Shader Device::createShader(const std::vector<char>& spirv)
	{
		return m ? m->createShader(spirv) : Shader{};
	}

	Pipeline Device::createPipeline(const PipelineShaders& shaders, const RenderTarget& target)
	{
		return createPipeline(shaders, target, PipelineConfig{});
	}

	Pipeline Device::createPipeline(const PipelineShaders& shaders, const RenderTarget& target, const PipelineConfig& config)
	{
		return m ? m->createPipeline(shaders, target, config) : Pipeline{};
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

	Texture Device::createTexture(Format format, Extent2D extent, const TextureConfig& config)
	{
		return m ? m->createTexture(format, extent, config) : Texture{};
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

	const DeviceInfo& Device::deviceInfo() const
	{
		return m->deviceInfo();
	}

	uint32_t Device::queueFamily() const
	{
		return m->queueFamily();
	}

	const DeviceFeatures& Device::enabledFeatures() const
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

	Device::Device(std::unique_ptr<backend::Device> device)
	{
		if (!device->valid())
		{
			m_error = device->error();
			return;
		}
		m = std::move(device);
	}

} // namespace acm
