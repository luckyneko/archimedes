/*
 *  Created by LuckyNeko on 21/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

// Internal (not installed) acm<->Vk mapping helpers. Single source of truth for
// translating the backend-neutral types in <archimedes/acmTypes.h> to and from
// their Vulkan equivalents. Included only by library .cpp files, which already
// pull in <vulkan/vulkan.h>.

#include "archimedes/acmLog.h"
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
	// Bytes one tightly packed texel of `format` occupies (0 for Undefined). For
	// D24_Unorm_S8_Uint this is the packed 32-bit texel, not a per-aspect copy size.
	uint32_t bytesPerPixel(acm::Format format);
	// The enumerator's name (e.g. "R8G8B8A8_Unorm"), for messages.
	const char* formatName(acm::Format format);
	// Whether `format` is a depth (or depth-stencil) format.
	bool isDepthFormat(acm::Format format);

	// The number of samples per pixel (1, 2, 4 or 8).
	uint32_t sampleCountValue(acm::SampleCount count);

	VkColorSpaceKHR toVk(acm::ColorSpace colorSpace);
	bool tryFromVk(VkColorSpaceKHR colorSpace, acm::ColorSpace& out);

	VkPresentModeKHR toVk(acm::PresentMode mode);
	bool tryFromVk(VkPresentModeKHR mode, acm::PresentMode& out);

	acm::PhysicalDeviceType fromVk(VkPhysicalDeviceType type);

	// A debug-messenger message's severity and source, as the log handler sees them.
	acm::LogSeverity toLogSeverity(VkDebugUtilsMessageSeverityFlagBitsEXT severity);
	acm::LogCategory toLogCategory(VkDebugUtilsMessageTypeFlagsEXT type);

	VkBufferUsageFlags toVk(acm::BufferUsage usage);
	// The enumerator's name (e.g. "Uniform"), for messages.
	const char* bufferUsageName(acm::BufferUsage usage);

	VkDescriptorType toVk(acm::DescriptorType type);
	// The enumerator's name (e.g. "StorageBuffer"), for messages.
	const char* descriptorTypeName(acm::DescriptorType type);
	VkShaderStageFlags toVk(acm::ShaderStage stage);
	VkPipelineStageFlags2 toVkPipelineStage(acm::ShaderStage stage);

	struct VkLayoutInfo
	{
		VkImageLayout layout;
		VkAccessFlags2 access;
		VkPipelineStageFlags2 stage;
	};
	VkLayoutInfo toVk(acm::ImageLayout layout);

	VkPrimitiveTopology toVk(acm::Topology topology);
	VkCullModeFlags toVk(acm::CullMode cull);
	VkFrontFace toVk(acm::FrontFace front);
	VkPolygonMode toVk(acm::PolygonMode mode);
	VkCompareOp toVk(acm::CompareOp op);
} // namespace acm::vulkan
