/*
 *  Created by LuckyNeko on 05/07/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/acmRuntime.h"

#include "archimedes/Platform.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>

#ifndef ACM_MOLTENVK_LOG_LEVEL
#	define ACM_MOLTENVK_LOG_LEVEL "1"
#endif

namespace
{
	// The most worker threads the CPU driver's scheduler holds (marl's MaxWorkerThreads).
	constexpr uint32_t MaxCpuDeviceThreads = 256;
} // namespace

namespace acm
{
	void useStagedVulkanRuntime(const RuntimeConfig& config)
	{
		// Read by the staged CPU driver (luckyneko/swiftshader-prebuilt's patched SwiftShader),
		// which acm::Instance loads directly; it needs no other environment.
		if (config.cpuDeviceThreads > 0)
			setEnvDefault("SWIFTSHADER_THREAD_COUNT", std::to_string(std::min(config.cpuDeviceThreads, MaxCpuDeviceThreads)));

#if defined(__APPLE__)
		setEnvDefault("MVK_CONFIG_LOG_LEVEL", ACM_MOLTENVK_LOG_LEVEL);

		// MoltenVK goes through the loader's own discovery, not direct loading: GLFW must see
		// it (and its surface extensions) before an instance exists, and MoltenVK 1.4.1 speaks
		// loader-driver interface 5, below direct loading's 7. The staged driver is the whole
		// list, as it always was.
		const std::filesystem::path executableDir = executableDirectory();
		if (executableDir.empty())
			return;
		const std::filesystem::path moltenVK = executableDir / "vulkan" / "MoltenVK_icd.json";
		std::error_code ec;
		if (std::filesystem::exists(moltenVK, ec))
			setEnvDefault("VK_ICD_FILENAMES", moltenVK.string());
#endif
	}
} // namespace acm
