/*
 *  Created by LuckyNeko on 24/10/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include <filesystem>
#include <utility>

namespace acm
{
	// Move-only RAII owner of a dynamically loaded library (e.g. a directly loaded Vulkan
	// driver). Unloads it on reset/destruction; empty (valid() == false) when loading
	// failed. The native handle is stored type-erased so this header stays free of
	// <windows.h>.
	class PlatformLibrary
	{
	public:
		PlatformLibrary() = default;
		explicit PlatformLibrary(const std::filesystem::path& path);
		~PlatformLibrary() { reset(); }

		PlatformLibrary(PlatformLibrary&& other) noexcept
			: m_handle(std::exchange(other.m_handle, nullptr))
		{
		}
		PlatformLibrary& operator=(PlatformLibrary&& other) noexcept
		{
			if (this != &other)
			{
				reset();
				m_handle = std::exchange(other.m_handle, nullptr);
			}
			return *this;
		}
		PlatformLibrary(const PlatformLibrary&) = delete;
		PlatformLibrary& operator=(const PlatformLibrary&) = delete;

		// Unloads the library; a no-op when empty.
		void reset();
		bool valid() const { return m_handle != nullptr; }
		// The exported symbol `name`, or nullptr when absent or empty.
		void* symbol(const char* name) const;

	private:
		void* m_handle{nullptr};
	};
} // namespace acm
