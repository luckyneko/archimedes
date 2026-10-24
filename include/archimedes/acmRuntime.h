/*
 *  Created by LuckyNeko on 05/07/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include <cstdint>

namespace acm
{
	// Runtime setup knobs for useStagedVulkanRuntime().
	struct RuntimeConfig
	{
		// Worker threads the CPU driver may run shaders on, so a host with its own
		// process-wide pool can split the machine with it. 0 keeps the driver's default (one
		// per logical CPU, at most 16); larger values are clamped to 256. The driver reads it
		// once, when the first CPU device is created, and a value the caller already set in
		// the environment (SWIFTSHADER_THREAD_COUNT) wins.
		uint32_t cpuDeviceThreads{0};
	};

	// Prepare the Vulkan runtime environment for a direct launch, before any other Vulkan or
	// GLFW use (GLFW enumerates instance extensions before an instance exists, so the loader
	// must already know its drivers). On macOS it points the loader at the MoltenVK staged next
	// to this executable (by the CMake helper acm_stage_vulkan_runtime) through
	// VK_ICD_FILENAMES, and defaults MoltenVK's log level to the ARCHIMEDES_MOLTENVK_LOG_LEVEL
	// CMake cache var. The staged CPU driver needs nothing here — acm::Instance loads it
	// directly (InstanceConfig::cpuDriver) — except the optional thread cap in `config`. A
	// value the caller already set in the environment is never overwritten.
	void useStagedVulkanRuntime(const acm::RuntimeConfig& config = {});
} // namespace acm
