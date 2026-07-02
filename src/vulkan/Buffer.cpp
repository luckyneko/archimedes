/*
 *  Created by LuckyNeko on 24/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#include "archimedes/vulkan/Buffer.h"

#include "archimedes/acmBuffer.h"
#include "archimedes/vulkan/Convert.h"
#include "archimedes/vulkan/Device.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace acm::vulkan
{

	// -----------------------------------------------------------------------------
	// Lifetime
	// -----------------------------------------------------------------------------

	Buffer::Buffer(Device& owner, size_t size, acm::BufferUsage usage)
	{
		if (size == 0)
		{
			m_error = acm::Error("failed to create buffer with zero size");
			return;
		}
		m_owner = &owner;
		m_size = size;
		m_hostVisible = isHostVisible(usage);

		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = toVk(usage);
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		if (vkCreateBuffer(owner.vkDevice(), &bufferInfo, nullptr, &m_buffer) != VK_SUCCESS)
		{
			m_error = acm::Error("failed to create buffer");
			return;
		}

		const VkMemoryPropertyFlags properties = m_hostVisible
													 ? (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
													 : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		VkMemoryRequirements requirements;
		vkGetBufferMemoryRequirements(owner.vkDevice(), m_buffer, &requirements);
		m_allocation = owner.allocator().allocate(requirements, properties);
		if (!m_allocation.valid())
		{
			m_error = acm::Error("failed to allocate buffer memory");
			return;
		}
		vkBindBufferMemory(owner.vkDevice(), m_buffer, m_allocation.memory, m_allocation.offset);
	}

	Buffer::~Buffer()
	{
		release();
	}

	Buffer::Buffer(Buffer&& other) noexcept
	{
		*this = std::move(other);
	}

	Buffer& Buffer::operator=(Buffer&& other) noexcept
	{
		if (this == &other)
			return *this;
		release();
		m_owner = std::exchange(other.m_owner, nullptr);
		m_size = std::exchange(other.m_size, 0);
		m_hostVisible = std::exchange(other.m_hostVisible, true);
		m_buffer = std::exchange(other.m_buffer, VK_NULL_HANDLE);
		m_allocation = std::exchange(other.m_allocation, {});
		m_error = std::move(other.m_error);
		return *this;
	}

	// -----------------------------------------------------------------------------
	// State
	// -----------------------------------------------------------------------------

	size_t Buffer::size() const
	{
		return m_size;
	}

	void* Buffer::map()
	{
		return m_hostVisible ? m_allocation.mapped : nullptr;
	}

	VkBuffer Buffer::vkBuffer() const
	{
		return m_buffer;
	}

	acm::Error Buffer::write(const void* data, size_t size)
	{
		const size_t bytes = std::min(size, m_size);
		if (m_hostVisible)
		{
			if (!m_allocation.mapped)
				return acm::Error("Buffer::write: map failed");
			std::memcpy(m_allocation.mapped, data, bytes);
			return {};
		}

		acm::Buffer staging = owner().createBuffer(bytes, acm::BufferUsage::Staging);
		if (!staging.valid())
			return acm::Error("Buffer::write: failed to create staging buffer");
		if (acm::Error error = staging.native()->write(data, bytes))
			return error;
		return owner().copyBuffer(staging.native()->vkBuffer(), m_buffer, VkDeviceSize(bytes));
	}

	// -----------------------------------------------------------------------------
	// Internals
	// -----------------------------------------------------------------------------

	bool Buffer::isHostVisible(acm::BufferUsage usage)
	{
		return usage == acm::BufferUsage::Uniform || usage == acm::BufferUsage::TransferDst || usage == acm::BufferUsage::Staging || usage == acm::BufferUsage::Storage;
	}

	void Buffer::release()
	{
		Device* owner = std::exchange(m_owner, nullptr);
		const VkBuffer buffer = std::exchange(m_buffer, VK_NULL_HANDLE);
		const Allocation allocation = std::exchange(m_allocation, {});
		m_size = 0;
		m_hostVisible = true;
		if (!owner)
			return;
		if (buffer)
			vkDestroyBuffer(owner->vkDevice(), buffer, nullptr);
		if (allocation.valid())
			owner->allocator().free(allocation);
	}

} // namespace acm::vulkan
