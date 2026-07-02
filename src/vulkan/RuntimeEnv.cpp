#include "archimedes/vulkan/RuntimeEnv.h"

#ifndef ACM_MOLTENVK_LOG_LEVEL
#	define ACM_MOLTENVK_LOG_LEVEL "1"
#endif

#if defined(__APPLE__)
#	include <cstdlib>
#	include <filesystem>
#	include <mach-o/dyld.h>
#	include <string>
#endif

void acm::vulkan::useStagedVulkanICD()
{
#if defined(__APPLE__)
	setenv("MVK_CONFIG_LOG_LEVEL", ACM_MOLTENVK_LOG_LEVEL, 0); // 0: do not overwrite

	if (!std::getenv("VK_ICD_FILENAMES"))
	{
		uint32_t size = 0;
		_NSGetExecutablePath(nullptr, &size); // first call reports required size
		std::string pathBuffer(size, '\0');
		if (_NSGetExecutablePath(pathBuffer.data(), &size) != 0)
			return;

		std::error_code ec;
		std::filesystem::path executable = std::filesystem::canonical(pathBuffer.c_str(), ec);
		if (ec)
			return;

		const std::filesystem::path icd = executable.parent_path() / "vulkan" / "MoltenVK_icd.json";
		if (std::filesystem::exists(icd))
			setenv("VK_ICD_FILENAMES", icd.string().c_str(), 0); // 0: do not overwrite
	}
#endif
}
