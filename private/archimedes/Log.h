/*
 *  Created by LuckyNeko on 22/11/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmLog.h"

#include <string>

// The library side of the log hook (include/archimedes/acmLog.h): how Archimedes code
// reports. Each call delivers one message to the installed handler, synchronously, on the
// calling thread. It is thread-safe and never throws, so it is safe anywhere — a render
// thread, a destructor, a Vulkan callback.
//
// Report through the category helpers below; reserve logMessage() for relaying another
// source (the Vulkan debug messenger). Write one self-contained line naming the object and
// what happened instead, without a trailing newline, e.g. for a degrade-with-a-warning
// fallback:
//
//     acm::logWarning("pipeline: wideLines is not enabled on this device; lineWidth 4 falls back to 1");
//
// Use Warning when a request was degraded or ignored and the caller should know, Info for
// a notable but expected event, Verbose for detail only a debugging session wants, and
// Error for a failure (normally alongside the acm::Error the caller gets back).
namespace acm
{
	// Delivers one message. `text` and `id` may be null (treated as empty).
	void logMessage(acm::LogSeverity severity, acm::LogCategory category, const char* text, const char* id = "") noexcept;

	inline void logMessage(acm::LogSeverity severity, acm::LogCategory category, const std::string& text, const std::string& id = {}) noexcept
	{
		acm::logMessage(severity, category, text.c_str(), id.c_str());
	}

	// Archimedes' own messages (LogCategory::Library) at each severity.
	inline void logVerbose(const std::string& text) noexcept
	{
		acm::logMessage(acm::LogSeverity::Verbose, acm::LogCategory::Library, text.c_str());
	}

	inline void logInfo(const std::string& text) noexcept
	{
		acm::logMessage(acm::LogSeverity::Info, acm::LogCategory::Library, text.c_str());
	}

	inline void logWarning(const std::string& text) noexcept
	{
		acm::logMessage(acm::LogSeverity::Warning, acm::LogCategory::Library, text.c_str());
	}

	inline void logError(const std::string& text) noexcept
	{
		acm::logMessage(acm::LogSeverity::Error, acm::LogCategory::Library, text.c_str());
	}
} // namespace acm
