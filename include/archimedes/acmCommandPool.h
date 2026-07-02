/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmForward.h"
#include "archimedes/acmResourceRef.h"
#include "archimedes/acmNative.h"

namespace acm
{
	// Copyable handle to a native command pool. It owns the allocation arena for
	// CommandBuffers; keep the pool alive while allocated command-buffer handles exist.
	class CommandPool
	{
	public:
		// Lifetime
		CommandPool();
		CommandPool(const acm::CommandPool& other);
		CommandPool& operator=(const acm::CommandPool& other);
		CommandPool(acm::CommandPool&& other) noexcept;
		CommandPool& operator=(acm::CommandPool&& other) noexcept;
		~CommandPool();

		// State
		void reset();
		bool valid() const;
		acm::Error error() const;
		acm::native::CommandPool* native() const;

		// Allocation
		acm::CommandBuffer allocate();

	private:
		// Construction
		friend acm::native::Device;
		CommandPool(acm::ResourceRef<acm::native::CommandPool> resource);
		explicit CommandPool(acm::Error error);

		acm::ResourceRef<acm::native::CommandPool> m_resource;
		acm::Error m_error;
	};
} // namespace acm
