/*
 *  Created by LuckyNeko on 24/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmError.h"

#include <vulkan/vulkan.h>

namespace acm::vulkan
{
	class Device;

	class CommandPool
	{
	public:
		CommandPool() = default;
		explicit CommandPool(acm::vulkan::Device& owner);
		~CommandPool();
		CommandPool(const CommandPool&) = delete;
		CommandPool& operator=(const CommandPool&) = delete;
		CommandPool(CommandPool&& other) noexcept;
		CommandPool& operator=(CommandPool&& other) noexcept;

		acm::vulkan::Device& owner() const { return *m_owner; }
		bool valid() const { return m_owner && m_pool != VK_NULL_HANDLE && m_error.ok(); }
		acm::Error error() const { return m_error; }
		VkCommandPool vkCommandPool() const;

	private:
		void release();

		acm::vulkan::Device* m_owner{nullptr};
		VkCommandPool m_pool{VK_NULL_HANDLE};
		acm::Error m_error;
	};
} // namespace acm::vulkan
