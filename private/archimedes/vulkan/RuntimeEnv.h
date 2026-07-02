/*
 *  Created by LuckyNeko on 02/07/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

namespace acm::vulkan
{
	// Direct IDE launches skip the generated run_<target>.sh wrapper that exports
	// VK_ICD_FILENAMES. On macOS, point the Vulkan loader at the MoltenVK ICD staged
	// next to this executable unless the caller already chose an ICD. Also default
	// MoltenVK logging below info-level unless the caller already set it.
	void useStagedVulkanICD();
} // namespace acm::vulkan
