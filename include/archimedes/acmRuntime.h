/*
 *  Created by LuckyNeko on 05/07/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

namespace acm
{
	// Prepare the Vulkan runtime environment for a direct launch. On macOS, point the
	// loader at the MoltenVK ICD staged next to this executable (by the CMake helper
	// acm_stage_vulkan_runtime) and default MoltenVK's log level, so an app finds the
	// driver without a system Vulkan install or a hand-set VK_ICD_FILENAMES. Call once,
	// before any other Vulkan or GLFW use (GLFW enumerates instance extensions before an
	// instance exists, so the loader must already know where the ICD is).
	//
	// Both settings respect a value the caller already chose (VK_ICD_FILENAMES /
	// MVK_CONFIG_LOG_LEVEL are never overwritten); the log-level default is the
	// ARCHIMEDES_MOLTENVK_LOG_LEVEL CMake cache var. A no-op on non-Apple platforms,
	// where the loader discovers system-registered ICDs.
	void useStagedVulkanRuntime();
} // namespace acm
