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

acm::Device::Device() = default;

acm::Device::Device(std::unique_ptr<acm::native::Device> device)
{
	if (!device->valid())
	{
		m_error = device->error();
		return;
	}
	m = std::move(device);
}

acm::Device::Device(acm::Device&& other) noexcept = default;

acm::Device& acm::Device::operator=(acm::Device&& other) noexcept = default;

acm::Device::~Device() = default;

void acm::Device::reset()
{
	m.reset();
	m_error = {};
}

bool acm::Device::valid() const
{
	return m != nullptr;
}

acm::Error acm::Device::error() const
{
	return m_error;
}

acm::SwapChain acm::Device::createSwapChain(const acm::Surface& surface, acm::SurfaceFormat format, acm::PresentMode presentMode, acm::Extent2D desiredExtent, bool depth, acm::SampleCount samples)
{
	return m ? m->createSwapChain(surface, format, presentMode, desiredExtent, depth, samples) : acm::SwapChain{};
}

acm::RenderTarget acm::Device::createRenderTarget(const acm::Texture& texture, acm::RenderTargetFinish finish, bool depth, acm::SampleCount samples)
{
	return m ? m->createRenderTarget(texture, finish, depth, samples) : acm::RenderTarget{};
}

acm::Texture acm::Device::createTexture(acm::Format format, acm::Extent2D extent, bool mipmapped, bool storage)
{
	return m ? m->createTexture(format, extent, mipmapped, storage) : acm::Texture{};
}

acm::Buffer acm::Device::createBuffer(size_t size, acm::BufferUsage usage)
{
	return m ? m->createBuffer(size, usage) : acm::Buffer{};
}

acm::Sampler acm::Device::createSampler(float maxAnisotropy)
{
	return m ? m->createSampler(maxAnisotropy) : acm::Sampler{};
}

acm::DescriptorSetLayout acm::Device::createDescriptorSetLayout(uint32_t samplerCount)
{
	std::vector<acm::DescriptorBinding> bindings(samplerCount);
	for (uint32_t i = 0; i < samplerCount; ++i)
		bindings[i] = {i, acm::DescriptorType::CombinedImageSampler, acm::ShaderStage::Fragment};
	return m ? m->createDescriptorSetLayout(bindings) : acm::DescriptorSetLayout{};
}

acm::DescriptorSetLayout acm::Device::createDescriptorSetLayout(const std::vector<acm::DescriptorBinding>& bindings)
{
	return m ? m->createDescriptorSetLayout(bindings) : acm::DescriptorSetLayout{};
}

acm::DescriptorSet acm::Device::createDescriptorSet(const acm::DescriptorSetLayout& layout)
{
	return m ? m->createDescriptorSet(layout) : acm::DescriptorSet{};
}

acm::Shader acm::Device::createShader(const std::vector<char>& spirv)
{
	return m ? m->createShader(spirv) : acm::Shader{};
}

acm::Pipeline acm::Device::createPipeline(const acm::Shader& vertex, const acm::Shader& fragment, const acm::RenderTarget& target)
{
	acm::PipelineConfig config;
	config.vertex = vertex;
	config.fragment = fragment;
	config.target = target;
	return m ? m->createPipeline(config) : acm::Pipeline{};
}

acm::Pipeline acm::Device::createPipeline(const acm::PipelineConfig& config)
{
	return m ? m->createPipeline(config) : acm::Pipeline{};
}

acm::ComputePipeline acm::Device::createComputePipeline(const acm::Shader& compute, const acm::DescriptorSetLayout& layout)
{
	return m ? m->createComputePipeline(compute, layout) : acm::ComputePipeline{};
}

acm::CommandPool acm::Device::createCommandPool()
{
	return m ? m->createCommandPool() : acm::CommandPool{};
}

acm::Renderer acm::Device::createRenderer(const acm::SwapChain& swapChain)
{
	return m ? m->createRenderer(swapChain) : acm::Renderer{};
}

const acm::GPU& acm::Device::getGPU() const
{
	return m->gpu();
}

uint32_t acm::Device::getQueueIdx() const
{
	return m->queueIndex();
}

const acm::GPUFeatures& acm::Device::enabledFeatures() const
{
	return m->enabledFeatures();
}

acm::SampleCount acm::Device::maxSampleCount() const
{
	return m->maxSampleCount();
}

size_t acm::Device::minUniformBufferOffsetAlignment() const
{
	return m->minUniformBufferOffsetAlignment();
}

size_t acm::Device::memoryBlockCount() const
{
	return m->memoryBlockCount();
}

void acm::Device::waitIdle()
{
	m->waitIdle();
}

acm::Error acm::Device::submitSync(const std::function<void(acm::CommandBuffer&)>& record)
{
	acm::CommandPool pool = createCommandPool();
	acm::CommandBuffer commandBuffer = pool.allocate();
	if (!commandBuffer.valid())
		return acm::Error("submitSync: failed to allocate command buffer");

	if (auto error = commandBuffer.begin())
		return error;
	record(commandBuffer);
	if (auto error = commandBuffer.end())
		return error;

	return submitSync(commandBuffer);
}

acm::Error acm::Device::submitSync(const acm::CommandBuffer& commandBuffer)
{
	return m ? m->submitCommandBufferSync(commandBuffer) : acm::Error("submitSync: invalid device");
}
