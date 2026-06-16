#pragma once

// Forward declarations of the Vulkan *handle* types that appear in acm's public
// accessors / raw-handle constructors, so public headers need not pull in the
// whole <vulkan/vulkan.h>. Handles are opaque pointer typedefs; only code that
// also includes <vulkan/vulkan.h> can do anything with them.
//
// This is written to be guard-compatible with the real Vulkan headers: the
// macros are only defined if Vulkan hasn't already, and re-issuing a typedef
// that names the same type is benign in C++. We target 64-bit platforms (Apple
// Silicon), where non-dispatchable handles are pointers — matching vulkan.h. If
// a 32-bit target is ever added, this assumption must be revisited.

#ifndef VK_DEFINE_HANDLE
#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
#endif

#ifndef VK_DEFINE_NON_DISPATCHABLE_HANDLE
#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef struct object##_T* object;
#endif

VK_DEFINE_HANDLE(VkInstance)
VK_DEFINE_HANDLE(VkPhysicalDevice)
VK_DEFINE_HANDLE(VkDevice)
VK_DEFINE_HANDLE(VkQueue)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkSurfaceKHR)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkSwapchainKHR)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkRenderPass)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkFramebuffer)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkImage)
