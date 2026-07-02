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
	class CommandPool
	{
	public:
		CommandPool();
		CommandPool(const acm::CommandPool& other);
		CommandPool& operator=(const acm::CommandPool& other);
		CommandPool(acm::CommandPool&& other) noexcept;
		CommandPool& operator=(acm::CommandPool&& other) noexcept;
		~CommandPool();

		void reset();
		bool valid() const;
		acm::Error error() const;
		acm::CommandBuffer allocate();
		acm::native::CommandPool* native() const;

	private:
		friend acm::native::Device;
		CommandPool(acm::ResourceRef<acm::native::CommandPool> resource);
		explicit CommandPool(acm::Error error);

		acm::ResourceRef<acm::native::CommandPool> m_resource;
		acm::Error m_error;
	};
} // namespace acm
