#pragma once

// The selected backend's opaque platform-integration handles. Repeating these
// typedefs in the complete backend header is valid and does not alter that
// header's own handle-definition macros.
typedef struct VkInstance_T* VkInstance;
typedef struct VkSurfaceKHR_T* VkSurfaceKHR;

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

namespace acm::native
{
	using InstanceHandle = VkInstance;
	using SurfaceHandle = VkSurfaceKHR;

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
} // namespace acm::native
