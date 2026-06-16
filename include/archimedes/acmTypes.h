#pragma once

#include <cstdint>

namespace acm
{
	struct Extent2D
	{
		uint32_t width{ 0 };
		uint32_t height{ 0 };
	};

	struct Extent3D
	{
		uint32_t width{ 0 };
		uint32_t height{ 0 };
		uint32_t depth{ 1 };
	};

	// Backend-neutral image formats. This is a deliberately curated subset of
	// what Vulkan exposes (~250 VkFormat values); it covers the formats the
	// renderer actually surfaces today. Anything outside the set maps to
	// Undefined (with a warning) in the acm<->Vk conversion layer; extend both
	// the enum and the tables in src/acmVkConvert.cpp as new formats are needed.
	enum class Format
	{
		Undefined,
		R8G8B8A8_Unorm,
		R8G8B8A8_Srgb,
		B8G8R8A8_Unorm,
		B8G8R8A8_Srgb,
		A2B10G10R10_Unorm_Pack32,
		R16G16B16A16_Sfloat,
		D32_Sfloat,
		D24_Unorm_S8_Uint,
	};

	enum class ColorSpace
	{
		SrgbNonlinear,
	};

	enum class PresentMode
	{
		Immediate,
		Mailbox,
		Fifo,
		FifoRelaxed,
	};

	enum class PhysicalDeviceType
	{
		Other,
		IntegratedGpu,
		DiscreteGpu,
		VirtualGpu,
		Cpu,
	};

	enum class ImageType
	{
		e1D,
		e2D,
		e3D,
	};

	struct SurfaceFormat
	{
		Format format{ Format::Undefined };
		ColorSpace colorSpace{ ColorSpace::SrgbNonlinear };
	};

	// Backend-neutral subset of VkSurfaceCapabilitiesKHR. The raw capabilities
	// (transform, usage flags, etc.) are kept inside Surface::impl for internal
	// swapchain creation; this is the public, descriptive view.
	struct SurfaceCapabilities
	{
		uint32_t minImageCount{ 0 };
		uint32_t maxImageCount{ 0 };
		Extent2D currentExtent;
		Extent2D minImageExtent;
		Extent2D maxImageExtent;
	};

	// Backend-neutral description of an image, enough to build views/attachments.
	struct ImageDesc
	{
		ImageType type{ ImageType::e2D };
		Format format{ Format::Undefined };
		Extent3D extent;
	};
}
