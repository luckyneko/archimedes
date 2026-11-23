/*
 *  Created by LuckyNeko on 22/11/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/acmLog.h"

#include <cstdio>

namespace acm
{
	void defaultLogHandler(const LogMessage& message)
	{
		if (message.severity < LogSeverity::Warning)
			return;
		// One call per line, so lines from concurrent threads do not interleave.
		if (message.id[0] != '\0')
			std::fprintf(stderr, "archimedes [%s %s] %s: %s\n", logCategoryName(message.category), logSeverityName(message.severity), message.id, message.text);
		else
			std::fprintf(stderr, "archimedes [%s %s]: %s\n", logCategoryName(message.category), logSeverityName(message.severity), message.text);
	}

	const char* logSeverityName(LogSeverity severity)
	{
		switch (severity)
		{
			case LogSeverity::Verbose:
				return "verbose";
			case LogSeverity::Info:
				return "info";
			case LogSeverity::Warning:
				return "warning";
			case LogSeverity::Error:
				return "error";
		}
		return "unknown";
	}

	const char* logCategoryName(LogCategory category)
	{
		switch (category)
		{
			case LogCategory::Library:
				return "library";
			case LogCategory::Vulkan:
				return "vulkan";
			case LogCategory::Validation:
				return "validation";
			case LogCategory::Performance:
				return "performance";
		}
		return "unknown";
	}
} // namespace acm
