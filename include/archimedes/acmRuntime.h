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
	// Prepare the Vulkan runtime environment for a direct launch: point the loader at the
	// drivers staged next to this executable (by the CMake helper acm_stage_vulkan_runtime)
	// so an app finds them without a system Vulkan install or a hand-set environment. The
	// staged drivers are MoltenVK on macOS and the CPU driver (SwiftShader) wherever it was
	// staged. Call once, before any other Vulkan or GLFW use (GLFW enumerates instance
	// extensions before an instance exists, so the loader must already know its drivers).
	//
	// macOS makes the staged drivers the whole list (VK_ICD_FILENAMES); Linux and Windows
	// keep system drivers and add the staged ones (VK_ADD_DRIVER_FILES). A value the caller
	// already set is never overwritten, and a caller-replaced driver list (VK_ICD_FILENAMES /
	// VK_DRIVER_FILES) is left alone. On macOS it also defaults MoltenVK's log level to the
	// ARCHIMEDES_MOLTENVK_LOG_LEVEL CMake cache var.
	void useStagedVulkanRuntime();
} // namespace acm
