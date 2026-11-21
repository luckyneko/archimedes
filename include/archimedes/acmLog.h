/*
 *  Created by LuckyNeko on 22/11/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include <cstdint>
#include <functional>

// The process-wide log hook. Archimedes reports through one handler: its own diagnostics
// (fallbacks, degraded features) and, on an instance created with InstanceConfig::debug,
// every message the Vulkan debug messenger delivers (validation layers, the loader, the
// driver). Nothing is buffered and there is no dependency: a message reaches the installed
// handler synchronously, on the thread that produced it.
namespace acm
{
	// How serious a message is. Ordered, so `message.severity >= LogSeverity::Warning` filters.
	enum class LogSeverity : uint8_t
	{
		Verbose, // diagnostic chatter (e.g. the Vulkan loader's search progress)
		Info,	 // a notable, expected event
		Warning, // something degraded or looks wrong; the operation continued
		Error,	 // a failure, or a Vulkan spec violation the validation layer caught
	};

	// Where a message came from.
	enum class LogCategory : uint8_t
	{
		Library,	 // Archimedes itself
		Vulkan,		 // a general message from the Vulkan loader, a layer or the driver
		Validation,	 // a validation-layer report; at LogSeverity::Error, a spec violation
		Performance, // a layer's performance warning (non-optimal but valid use)
	};

	// One message. The strings are never null and are valid only for the duration of the
	// handler call; copy them to keep them.
	struct LogMessage
	{
		acm::LogSeverity severity{acm::LogSeverity::Info};
		acm::LogCategory category{acm::LogCategory::Library};
		// A stable identifier when the source has one — a validation message's VUID name
		// (e.g. "VUID-vkCmdDraw-None-08600") — and empty otherwise.
		const char* id{""};
		// The message text, without a trailing newline.
		const char* text{""};
	};

	// Receives every message, of every severity and category. It may be called concurrently
	// from several threads (Vulkan reports on whichever thread made the offending call, and
	// Renderers run on their own threads), so it must be thread-safe itself. It must not
	// throw: an escaping exception is swallowed, since the caller may be a Vulkan callback.
	// It may log, call Vulkan, or call setLogHandler.
	using LogHandler = std::function<void(const acm::LogMessage& message)>;

	// Installs `handler` for the whole process and returns the handler it replaces, so a
	// caller can restore it later. An empty handler discards every message. Thread-safe; a
	// message already being delivered on another thread when this returns may still finish
	// in the old handler, so keep what an old handler references alive until no thread can
	// still be inside it.
	acm::LogHandler setLogHandler(acm::LogHandler handler);

	// The handler installed at startup: writes Warning and Error messages to stderr, one
	// line each, and ignores Verbose and Info. A custom handler can forward to it.
	void defaultLogHandler(const acm::LogMessage& message);

	// Lower-case names for formatting ("warning", "validation").
	const char* logSeverityName(acm::LogSeverity severity);
	const char* logCategoryName(acm::LogCategory category);
} // namespace acm
