#pragma once

// Internal (not installed) acm<->Vk mapping helpers. Single source of truth for
// translating the backend-neutral types in <archimedes/acmTypes.h> to and from
// their Vulkan equivalents. Included only by library .cpp files, which already
// pull in <vulkan/vulkan.h>.

#include "archimedes/acmTypes.h"

#include <vulkan/vulkan.h>

namespace acm::vulkan
{
	// toVk translates an app-supplied value; it warns on anything it cannot
	// map, since that means the caller set an invalid value. fromVk/tryFromVk
	// translate values discovered from the driver, where unmapped entries are
	// expected and benign, so they stay silent — tryFromVk just reports
	// whether the value is representable so callers can skip the rest.
	VkFormat toVk(acm::Format format);
	bool tryFromVk(VkFormat format, acm::Format& out);

	VkColorSpaceKHR toVk(acm::ColorSpace colorSpace);
	bool tryFromVk(VkColorSpaceKHR colorSpace, acm::ColorSpace& out);

	VkPresentModeKHR toVk(acm::PresentMode mode);
	bool tryFromVk(VkPresentModeKHR mode, acm::PresentMode& out);

	acm::PhysicalDeviceType fromVk(VkPhysicalDeviceType type);

	VkBufferUsageFlags toVk(acm::BufferUsage usage);

	VkDescriptorType toVk(acm::DescriptorType type);
	VkShaderStageFlags toVk(acm::ShaderStage stage);
	VkPipelineStageFlags toVkPipelineStage(acm::ShaderStage stage);

	struct VkLayoutInfo
	{
		VkImageLayout layout;
		VkAccessFlags access;
		VkPipelineStageFlags stage;
	};
	VkLayoutInfo toVk(acm::ImageLayout layout);

	VkPrimitiveTopology toVk(acm::Topology topology);
	VkCullModeFlags toVk(acm::CullMode cull);
	VkFrontFace toVk(acm::FrontFace front);
	VkPolygonMode toVk(acm::PolygonMode mode);

	// Clamps a requested sample count to what `phys` supports for color+depth
	// framebuffers (so both RenderTarget and Pipeline derive the same value from the
	// same request). maxSampleCount returns the device's highest usable count.
	VkSampleCountFlagBits toVkSampleCount(acm::SampleCount requested, VkPhysicalDevice phys);
	acm::SampleCount maxSampleCount(VkPhysicalDevice phys);
} // namespace acm::vulkan
