/*
 *  Created by LuckyNeko on 05/07/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/acmRuntime.h"

#include "archimedes/vulkan/PlatformSurface.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

#ifndef ACM_MOLTENVK_LOG_LEVEL
#	define ACM_MOLTENVK_LOG_LEVEL "1"
#endif

namespace
{
	// Sets an environment variable unless the caller already chose a value.
	void setEnvDefault(const char* name, const std::string& value)
	{
		if (std::getenv(name))
			return;
#ifdef _WIN32
		_putenv_s(name, value.c_str());
#else
		setenv(name, value.c_str(), 0); // 0: do not overwrite
#endif
	}

#ifdef _WIN32
	constexpr char PathListSeparator = ';';
#else
	constexpr char PathListSeparator = ':';
#endif

	// The ICD manifests acm_stage_vulkan_runtime can place in <bindir>/vulkan/.
	constexpr const char* StagedManifests[] = {
		"MoltenVK_icd.json",	   // macOS portability driver
		"vk_swiftshader_icd.json", // CPU driver
	};
} // namespace

namespace acm
{
	void useStagedVulkanRuntime()
	{
#if defined(__APPLE__)
		setEnvDefault("MVK_CONFIG_LOG_LEVEL", ACM_MOLTENVK_LOG_LEVEL);
#endif

		const std::filesystem::path executableDir = vulkan::executableDirectory();
		if (executableDir.empty())
			return;

		std::string staged;
		for (const char* manifest : StagedManifests)
		{
			const std::filesystem::path path = executableDir / "vulkan" / manifest;
			std::error_code ec;
			if (!std::filesystem::exists(path, ec))
				continue;
			if (!staged.empty())
				staged += PathListSeparator;
			staged += path.string();
		}
		if (staged.empty())
			return;

#if defined(__APPLE__)
		// The staged drivers are the whole list, as MoltenVK's always was.
		setEnvDefault("VK_ICD_FILENAMES", staged);
#else
		// Keep system GPU drivers visible and add the staged ones. The loader ignores the
		// addition when the caller replaced the driver list, so leave that choice alone.
		if (!std::getenv("VK_ICD_FILENAMES") && !std::getenv("VK_DRIVER_FILES"))
			setEnvDefault("VK_ADD_DRIVER_FILES", staged);
#endif
	}
} // namespace acm
