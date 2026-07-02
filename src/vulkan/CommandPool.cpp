/*
 *  Created by LuckyNeko on 24/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/vulkan/CommandPool.h"

#include "archimedes/vulkan/Device.h"

#include <utility>

namespace acm::vulkan
{

	// -----------------------------------------------------------------------------
	// Lifetime
	// -----------------------------------------------------------------------------

	CommandPool::CommandPool(Device& owner)
	{
		m_owner = &owner;
		VkCommandPoolCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		createInfo.queueFamilyIndex = owner.queueIndex();
		if (vkCreateCommandPool(owner.vkDevice(), &createInfo, nullptr, &m_pool) != VK_SUCCESS)
			m_error = acm::Error("failed to create command pool");
	}

	CommandPool::~CommandPool()
	{
		release();
	}

	CommandPool::CommandPool(CommandPool&& other) noexcept
	{
		*this = std::move(other);
	}

	CommandPool& CommandPool::operator=(CommandPool&& other) noexcept
	{
		if (this == &other)
			return *this;
		release();
		m_owner = std::exchange(other.m_owner, nullptr);
		m_pool = std::exchange(other.m_pool, VK_NULL_HANDLE);
		m_error = std::move(other.m_error);
		return *this;
	}

	// -----------------------------------------------------------------------------
	// State
	// -----------------------------------------------------------------------------

	VkCommandPool CommandPool::vkCommandPool() const
	{
		return m_pool;
	}

	// -----------------------------------------------------------------------------
	// Internals
	// -----------------------------------------------------------------------------

	void CommandPool::release()
	{
		Device* owner = std::exchange(m_owner, nullptr);
		const VkCommandPool pool = std::exchange(m_pool, VK_NULL_HANDLE);
		if (!owner || !pool)
			return;
		vkDestroyCommandPool(owner->vkDevice(), pool, nullptr);
	}

} // namespace acm::vulkan
