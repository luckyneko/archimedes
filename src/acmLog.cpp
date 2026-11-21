/*
 *  Created by LuckyNeko on 22/11/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/acmLog.h"

#include "archimedes/Log.h"

#include <cstdio>
#include <memory>
#include <mutex>
#include <utility>

namespace
{
	// The installed handler. A message copies the pointer under the mutex and calls the
	// handler outside it, so deliveries run concurrently, a handler may log or swap the
	// handler itself, and a replaced handler stays alive until its last call returns.
	struct HandlerSlot
	{
		std::mutex mutex;
		std::shared_ptr<const acm::LogHandler> handler{std::make_shared<const acm::LogHandler>(acm::defaultLogHandler)};
	};

	// Never destroyed, so logging stays safe during static destruction.
	HandlerSlot& handlerSlot()
	{
		static HandlerSlot* slot = new HandlerSlot;
		return *slot;
	}
} // namespace

namespace acm
{
	LogHandler setLogHandler(LogHandler handler)
	{
		auto next = std::make_shared<const LogHandler>(std::move(handler));
		std::shared_ptr<const LogHandler> previous;
		{
			HandlerSlot& slot = handlerSlot();
			std::lock_guard<std::mutex> lock(slot.mutex);
			previous = std::exchange(slot.handler, std::move(next));
		}
		return *previous;
	}

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

	void logMessage(LogSeverity severity, LogCategory category, const char* text, const char* id) noexcept
	{
		try
		{
			std::shared_ptr<const LogHandler> handler;
			{
				HandlerSlot& slot = handlerSlot();
				std::lock_guard<std::mutex> lock(slot.mutex);
				handler = slot.handler;
			}
			if (!handler || !*handler)
				return;

			LogMessage message;
			message.severity = severity;
			message.category = category;
			message.id = id ? id : "";
			message.text = text ? text : "";
			(*handler)(message);
		}
		catch (...)
		{
			// A handler must not throw; the caller may be a Vulkan callback.
		}
	}
} // namespace acm
