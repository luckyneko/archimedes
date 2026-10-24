/*
 *  Created by LuckyNeko on 24/10/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include <filesystem>
#include <string>

// Operating-system glue with no backend in it. src/Platform.cpp is one of the platform
// translation units allowed to include <windows.h>, so callers stay free of it.
namespace acm
{
	// The directory holding the running executable (symlinks resolved), or empty when the
	// platform cannot report it. Staged drivers live in its vulkan/ subfolder.
	std::filesystem::path executableDirectory();

	// Sets an environment variable unless the caller already chose a value.
	void setEnvDefault(const char* name, const std::string& value);
} // namespace acm
