/*
 *  Created by LuckyNeko on 24/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

// Backend-selection aliases. This header names NO Vulkan handle types: the mainline
// public API is Vk-free, and every raw Vulkan type is confined to the opt-in interop
// header (acmVulkanInterop.h), which includes the real Vulkan headers.

namespace acm::vulkan
{
	class Buffer;
	class CommandBuffer;
	class CommandPool;
	class ComputePipeline;
	class DescriptorSet;
	class DescriptorSetLayout;
	class Device;
	class Instance;
	class Pipeline;
	class Renderer;
	class RenderTarget;
	class Sampler;
	class Shader;
	class Surface;
	class SwapChain;
	class Texture;
} // namespace acm::vulkan

namespace acm::backend
{
	using Buffer = acm::vulkan::Buffer;
	using CommandBuffer = acm::vulkan::CommandBuffer;
	using CommandPool = acm::vulkan::CommandPool;
	using ComputePipeline = acm::vulkan::ComputePipeline;
	using DescriptorSet = acm::vulkan::DescriptorSet;
	using DescriptorSetLayout = acm::vulkan::DescriptorSetLayout;
	using Device = acm::vulkan::Device;
	using Instance = acm::vulkan::Instance;
	using Pipeline = acm::vulkan::Pipeline;
	using Renderer = acm::vulkan::Renderer;
	using RenderTarget = acm::vulkan::RenderTarget;
	using Sampler = acm::vulkan::Sampler;
	using Shader = acm::vulkan::Shader;
	using Surface = acm::vulkan::Surface;
	using SwapChain = acm::vulkan::SwapChain;
	using Texture = acm::vulkan::Texture;
} // namespace acm::backend
