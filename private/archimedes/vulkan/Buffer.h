/*
 *  Created by LuckyNeko on 24/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#pragma once

#include "archimedes/acmError.h"
#include "archimedes/acmTypes.h"
#include "archimedes/vulkan/Memory.h"

#include <vulkan/vulkan.h>

#include <cstddef>

namespace acm::vulkan
{
	class Device;

	class Buffer
	{
	public:
		Buffer() = default;
		Buffer(acm::vulkan::Device& owner, size_t size, acm::BufferUsage usage);
		~Buffer();
		Buffer(const Buffer&) = delete;
		Buffer& operator=(const Buffer&) = delete;
		Buffer(Buffer&& other) noexcept;
		Buffer& operator=(Buffer&& other) noexcept;

		acm::vulkan::Device& owner() const { return *m_owner; }
		bool valid() const { return m_owner && m_buffer != VK_NULL_HANDLE && m_error.ok(); }
		acm::Error error() const { return m_error; }
		size_t size() const;
		void* map();
		VkBuffer vkBuffer() const;
		acm::Error write(const void* data, size_t size);

	private:
		static bool isHostVisible(acm::BufferUsage usage);
		void release();

		acm::vulkan::Device* m_owner{nullptr};
		size_t m_size{0};
		bool m_hostVisible{true};
		VkBuffer m_buffer{VK_NULL_HANDLE};
		acm::vulkan::Allocation m_allocation;
		acm::Error m_error;
	};
} // namespace acm::vulkan
