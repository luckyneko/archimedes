#pragma once

#include <archimedes/archimedes.h>

// Shared scaffolding for the [gpu] integration tests. Header-only (inline) so
// each test translation unit can include it without an extra link target.
namespace acmtest
{
	// Creates a windowless VkSurfaceKHR via VK_EXT_headless_surface (enabled by
	// the instance's VK_*_surface match), or VK_NULL_HANDLE if the driver lacks
	// it. The caller owns it — typically by handing it to acm::Surface.
	inline VkSurfaceKHR createHeadlessSurface(acm::Instance& instance)
	{
		auto fn = reinterpret_cast<PFN_vkCreateHeadlessSurfaceEXT>(
			vkGetInstanceProcAddr(instance.vkInstance(), "vkCreateHeadlessSurfaceEXT"));
		if(!fn)
			return VK_NULL_HANDLE;

		VkHeadlessSurfaceCreateInfoEXT info = {};
		info.sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT;

		VkSurfaceKHR surface = VK_NULL_HANDLE;
		if(fn(instance.vkInstance(), &info, nullptr, &surface) != VK_SUCCESS)
			return VK_NULL_HANDLE;
		return surface;
	}

	// First graphics-capable GPU and its queue family index; nullptr if none.
	// The pointer is valid for the lifetime of the instance.
	inline const acm::GPU* selectGraphicsGPU(const acm::Instance& instance, uint32_t& queueIdx)
	{
		for(const auto& gpu : instance.getAvailableGPUs())
			for(const auto& qf : gpu.queueFamilies)
				if(qf.supportsGraphics)
				{
					queueIdx = qf.index;
					return &gpu;
				}
		return nullptr;
	}
}
