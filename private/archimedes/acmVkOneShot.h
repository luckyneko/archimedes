#pragma once

// Internal (not installed) helper for synchronous one-shot GPU work — the
// transient command buffer + submit + wait-idle dance shared by the staging
// uploads (device-local buffers, texture pixels). Included only by library .cpp
// files, which already pull in <vulkan/vulkan.h>.

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include <functional>
#include <vulkan/vulkan.h>

namespace acm
{
	// Records `record` into a transient, one-time-submit command buffer, submits it
	// to the device's queue, and waits the queue idle before returning (so any
	// scratch resources the caller used are safe to drop). A load-time tool — it
	// stalls the queue, so it is not for per-frame work.
	acm::Error oneShotSubmit(acm::Device device, const std::function<void(VkCommandBuffer)>& record);
} // namespace acm
