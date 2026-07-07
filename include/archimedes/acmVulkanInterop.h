/*
 *  Created by LuckyNeko on 05/07/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

// The one opt-in Vulkan-interop seam. This is the ONLY public archimedes header that
// names raw Vulkan types: include it when you need to hand archimedes' native handles to
// an external Vulkan consumer (e.g. ImGui's imgui_impl_vulkan backend). The mainline
// acm:: API stays entirely Vulkan-free; all raw handles are gathered here.
//
// Including this header pulls in <vulkan/vulkan.h>. Every handle accessor returns
// VK_NULL_HANDLE / VK_FORMAT_UNDEFINED for an invalid or stale wrapper.

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"

#include <vulkan/vulkan.h>

#include <functional>

namespace acm::interop
{
	// Owning-root handles — valid for the life of the Instance / Device.
	VkInstance instance(const acm::Instance& inst);
	VkPhysicalDevice physicalDevice(const acm::Device& dev);
	VkDevice device(const acm::Device& dev);

	// Borrowed queue handle for init structs and external Vulkan integrations (e.g.
	// ImGui_ImplVulkan_InitInfo). Any external call that may submit to or wait on this
	// queue must be wrapped in withQueue().
	VkQueue queue(const acm::Device& dev);

	// Run raw Vulkan queue work under Archimedes' device queue mutex. Use this around
	// external backends that internally call vkQueueSubmit / vkQueueWaitIdle (ImGui's
	// font/texture upload path does this from ImGui_ImplVulkan_NewFrame). Do not call
	// Archimedes APIs that submit, present, or wait on the same device from inside the
	// callback.
	acm::Error withQueue(acm::Device& dev, const std::function<void(VkQueue)>& work);

	// Adopt a VkSurfaceKHR produced by external platform code (e.g.
	// glfwCreateWindowSurface) as an acm::Surface. Ownership of the VkSurfaceKHR moves
	// to Archimedes; the caller still owns and must keep alive the native window.
	acm::Surface adoptSurface(acm::Instance& inst, VkSurfaceKHR surface);

	// Resource handles — valid while the resource is.
	VkCommandBuffer commandBuffer(const acm::CommandBuffer& cmd);
	VkImageView imageView(const acm::Texture& tex);

	// The swapchain's color format, for a dynamic-rendering pipeline's
	// VkPipelineRenderingCreateInfo (archimedes renders without a VkRenderPass).
	VkFormat colorFormat(const acm::SwapChain& swapChain);
} // namespace acm::interop
