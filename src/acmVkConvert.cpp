#include "archimedes/acmVkConvert.h"

#include <cassert>

namespace acm
{
	VkFormat toVk(acm::Format format)
	{
		switch (format)
		{
			case acm::Format::R8G8B8A8_Unorm:
				return VK_FORMAT_R8G8B8A8_UNORM;
			case acm::Format::R8G8B8A8_Srgb:
				return VK_FORMAT_R8G8B8A8_SRGB;
			case acm::Format::B8G8R8A8_Unorm:
				return VK_FORMAT_B8G8R8A8_UNORM;
			case acm::Format::B8G8R8A8_Srgb:
				return VK_FORMAT_B8G8R8A8_SRGB;
			case acm::Format::A2B10G10R10_Unorm_Pack32:
				return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
			case acm::Format::R16G16B16A16_Sfloat:
				return VK_FORMAT_R16G16B16A16_SFLOAT;
			case acm::Format::D32_Sfloat:
				return VK_FORMAT_D32_SFLOAT;
			case acm::Format::D24_Unorm_S8_Uint:
				return VK_FORMAT_D24_UNORM_S8_UINT;
			case acm::Format::R32_Sfloat:
				return VK_FORMAT_R32_SFLOAT;
			case acm::Format::R32G32_Sfloat:
				return VK_FORMAT_R32G32_SFLOAT;
			case acm::Format::R32G32B32_Sfloat:
				return VK_FORMAT_R32G32B32_SFLOAT;
			case acm::Format::R32G32B32A32_Sfloat:
				return VK_FORMAT_R32G32B32A32_SFLOAT;
			case acm::Format::Undefined:
				return VK_FORMAT_UNDEFINED;
		}
		assert(false && "acm::toVk: invalid acm::Format");
		return VK_FORMAT_UNDEFINED;
	}

	bool tryFromVk(VkFormat format, acm::Format& out)
	{
		switch (format)
		{
			case VK_FORMAT_R8G8B8A8_UNORM:
				out = acm::Format::R8G8B8A8_Unorm;
				return true;
			case VK_FORMAT_R8G8B8A8_SRGB:
				out = acm::Format::R8G8B8A8_Srgb;
				return true;
			case VK_FORMAT_B8G8R8A8_UNORM:
				out = acm::Format::B8G8R8A8_Unorm;
				return true;
			case VK_FORMAT_B8G8R8A8_SRGB:
				out = acm::Format::B8G8R8A8_Srgb;
				return true;
			case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
				out = acm::Format::A2B10G10R10_Unorm_Pack32;
				return true;
			case VK_FORMAT_R16G16B16A16_SFLOAT:
				out = acm::Format::R16G16B16A16_Sfloat;
				return true;
			case VK_FORMAT_D32_SFLOAT:
				out = acm::Format::D32_Sfloat;
				return true;
			case VK_FORMAT_D24_UNORM_S8_UINT:
				out = acm::Format::D24_Unorm_S8_Uint;
				return true;
			case VK_FORMAT_R32_SFLOAT:
				out = acm::Format::R32_Sfloat;
				return true;
			case VK_FORMAT_R32G32_SFLOAT:
				out = acm::Format::R32G32_Sfloat;
				return true;
			case VK_FORMAT_R32G32B32_SFLOAT:
				out = acm::Format::R32G32B32_Sfloat;
				return true;
			case VK_FORMAT_R32G32B32A32_SFLOAT:
				out = acm::Format::R32G32B32A32_Sfloat;
				return true;
			case VK_FORMAT_UNDEFINED:
				out = acm::Format::Undefined;
				return true;
			default:
				return false;
		}
	}

	VkColorSpaceKHR toVk(acm::ColorSpace colorSpace)
	{
		switch (colorSpace)
		{
			case acm::ColorSpace::SrgbNonlinear:
				return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		}
		assert(false && "acm::toVk: invalid acm::ColorSpace");
		return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	}

	bool tryFromVk(VkColorSpaceKHR colorSpace, acm::ColorSpace& out)
	{
		switch (colorSpace)
		{
			case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
				out = acm::ColorSpace::SrgbNonlinear;
				return true;
			default:
				return false;
		}
	}

	VkPresentModeKHR toVk(acm::PresentMode mode)
	{
		switch (mode)
		{
			case acm::PresentMode::Immediate:
				return VK_PRESENT_MODE_IMMEDIATE_KHR;
			case acm::PresentMode::Mailbox:
				return VK_PRESENT_MODE_MAILBOX_KHR;
			case acm::PresentMode::Fifo:
				return VK_PRESENT_MODE_FIFO_KHR;
			case acm::PresentMode::FifoRelaxed:
				return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
		}
		assert(false && "acm::toVk: invalid acm::PresentMode");
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	bool tryFromVk(VkPresentModeKHR mode, acm::PresentMode& out)
	{
		switch (mode)
		{
			case VK_PRESENT_MODE_IMMEDIATE_KHR:
				out = acm::PresentMode::Immediate;
				return true;
			case VK_PRESENT_MODE_MAILBOX_KHR:
				out = acm::PresentMode::Mailbox;
				return true;
			case VK_PRESENT_MODE_FIFO_KHR:
				out = acm::PresentMode::Fifo;
				return true;
			case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
				out = acm::PresentMode::FifoRelaxed;
				return true;
			default:
				return false;
		}
	}

	acm::PhysicalDeviceType fromVk(VkPhysicalDeviceType type)
	{
		switch (type)
		{
			case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
				return acm::PhysicalDeviceType::IntegratedGpu;
			case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
				return acm::PhysicalDeviceType::DiscreteGpu;
			case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
				return acm::PhysicalDeviceType::VirtualGpu;
			case VK_PHYSICAL_DEVICE_TYPE_CPU:
				return acm::PhysicalDeviceType::Cpu;
			case VK_PHYSICAL_DEVICE_TYPE_OTHER:
				return acm::PhysicalDeviceType::Other;
			default:
				return acm::PhysicalDeviceType::Other;
		}
	}

	VkBufferUsageFlags toVk(acm::BufferUsage usage)
	{
		switch (usage)
		{
			// Vertex/index buffers are device-local and filled via staging, so they
			// are also transfer destinations.
			case acm::BufferUsage::Vertex:
				return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			case acm::BufferUsage::Index:
				return VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			case acm::BufferUsage::Uniform:
				return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			case acm::BufferUsage::TransferDst:
				return VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			case acm::BufferUsage::Staging:
				return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			case acm::BufferUsage::Storage:
				return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		}
		assert(false && "acm::toVk: invalid acm::BufferUsage");
		return 0;
	}

	VkDescriptorType toVk(acm::DescriptorType type)
	{
		switch (type)
		{
			case acm::DescriptorType::UniformBuffer:
				return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			case acm::DescriptorType::CombinedImageSampler:
				return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			case acm::DescriptorType::StorageBuffer:
				return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			case acm::DescriptorType::UniformBufferDynamic:
				return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
			case acm::DescriptorType::StorageImage:
				return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		}
		assert(false && "acm::toVk: invalid acm::DescriptorType");
		return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	}

	// ShaderStage is a flag set, so OR the matching Vk bits (a single value works too).
	VkShaderStageFlags toVk(acm::ShaderStage stage)
	{
		VkShaderStageFlags flags = 0;
		if (uint32_t(stage) & uint32_t(acm::ShaderStage::Vertex))
			flags |= VK_SHADER_STAGE_VERTEX_BIT;
		if (uint32_t(stage) & uint32_t(acm::ShaderStage::Fragment))
			flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
		if (uint32_t(stage) & uint32_t(acm::ShaderStage::Compute))
			flags |= VK_SHADER_STAGE_COMPUTE_BIT;
		assert(flags != 0 && "acm::toVk: empty acm::ShaderStage");
		return flags;
	}

	VkPrimitiveTopology toVk(acm::Topology topology)
	{
		switch (topology)
		{
			case acm::Topology::TriangleList:
				return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			case acm::Topology::TriangleStrip:
				return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
			case acm::Topology::LineList:
				return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
			case acm::Topology::LineStrip:
				return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
			case acm::Topology::PointList:
				return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		}
		assert(false && "acm::toVk: invalid acm::Topology");
		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	}

	VkCullModeFlags toVk(acm::CullMode cull)
	{
		switch (cull)
		{
			case acm::CullMode::None:
				return VK_CULL_MODE_NONE;
			case acm::CullMode::Back:
				return VK_CULL_MODE_BACK_BIT;
			case acm::CullMode::Front:
				return VK_CULL_MODE_FRONT_BIT;
		}
		assert(false && "acm::toVk: invalid acm::CullMode");
		return VK_CULL_MODE_NONE;
	}

	VkFrontFace toVk(acm::FrontFace front)
	{
		switch (front)
		{
			case acm::FrontFace::Clockwise:
				return VK_FRONT_FACE_CLOCKWISE;
			case acm::FrontFace::CounterClockwise:
				return VK_FRONT_FACE_COUNTER_CLOCKWISE;
		}
		assert(false && "acm::toVk: invalid acm::FrontFace");
		return VK_FRONT_FACE_CLOCKWISE;
	}

	VkPolygonMode toVk(acm::PolygonMode mode)
	{
		switch (mode)
		{
			case acm::PolygonMode::Fill:
				return VK_POLYGON_MODE_FILL;
			case acm::PolygonMode::Line:
				return VK_POLYGON_MODE_LINE;
		}
		assert(false && "acm::toVk: invalid acm::PolygonMode");
		return VK_POLYGON_MODE_FILL;
	}

	VkSampleCountFlagBits toVkSampleCount(acm::SampleCount requested, VkPhysicalDevice phys)
	{
		uint32_t want = 1;
		switch (requested)
		{
			case acm::SampleCount::One:
				want = 1;
				break;
			case acm::SampleCount::Two:
				want = 2;
				break;
			case acm::SampleCount::Four:
				want = 4;
				break;
			case acm::SampleCount::Eight:
				want = 8;
				break;
		}

		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(phys, &props);
		const VkSampleCountFlags supported = props.limits.framebufferColorSampleCounts & props.limits.framebufferDepthSampleCounts;

		// Highest supported count not exceeding the request.
		for (uint32_t s = want; s >= 2; s >>= 1)
		{
			if (supported & s)
				return VkSampleCountFlagBits(s);
		}
		return VK_SAMPLE_COUNT_1_BIT;
	}

	acm::SampleCount maxSampleCount(VkPhysicalDevice phys)
	{
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(phys, &props);
		const VkSampleCountFlags supported = props.limits.framebufferColorSampleCounts & props.limits.framebufferDepthSampleCounts;
		if (supported & VK_SAMPLE_COUNT_8_BIT)
			return acm::SampleCount::Eight;
		if (supported & VK_SAMPLE_COUNT_4_BIT)
			return acm::SampleCount::Four;
		if (supported & VK_SAMPLE_COUNT_2_BIT)
			return acm::SampleCount::Two;
		return acm::SampleCount::One;
	}
} // namespace acm
