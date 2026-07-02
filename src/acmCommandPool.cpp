/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/acmCommandPool.h"

#include "archimedes/acmCommandBuffer.h"
#include "archimedes/backendAPI.h"

#include <utility>

namespace acm
{
	// -----------------------------------------------------------------------------
	// Lifetime
	// -----------------------------------------------------------------------------

	CommandPool::CommandPool() = default;

	CommandPool::CommandPool(const CommandPool& other) = default;

	CommandPool& CommandPool::operator=(const CommandPool& other) = default;

	CommandPool::CommandPool(CommandPool&& other) noexcept = default;

	CommandPool& CommandPool::operator=(CommandPool&& other) noexcept = default;

	CommandPool::~CommandPool() = default;

	// -----------------------------------------------------------------------------
	// State
	// -----------------------------------------------------------------------------

	void CommandPool::reset()
	{
		m_resource.reset();
		m_error = {};
	}

	bool CommandPool::valid() const
	{
		return m_resource.valid();
	}

	Error CommandPool::error() const
	{
		return m_error;
	}

	backend::CommandPool* CommandPool::backend() const
	{
		return m_resource.access();
	}

	// -----------------------------------------------------------------------------
	// Allocation
	// -----------------------------------------------------------------------------

	CommandBuffer CommandPool::allocate()
	{
		if (auto* resource = m_resource.access())
			return resource->owner().allocateCommandBuffer(*this);
		return CommandBuffer{};
	}

	// -----------------------------------------------------------------------------
	// Construction
	// -----------------------------------------------------------------------------

	CommandPool::CommandPool(ResourceRef<backend::CommandPool> resource)
		: m_resource(std::move(resource))
	{
	}

	CommandPool::CommandPool(Error error)
		: m_error(std::move(error))
	{
	}

} // namespace acm
