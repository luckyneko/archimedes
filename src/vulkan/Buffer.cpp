#include "archimedes/vulkan/Buffer.h"

#include "archimedes/acmBuffer.h"
#include "archimedes/vulkan/Convert.h"
#include "archimedes/vulkan/Device.h"

#include <algorithm>
#include <cstring>

bool acm::vulkan::Buffer::create(acm::vulkan::Device& owner, size_t size, acm::BufferUsage usage)
{
	if (size == 0)
		return false;
	m_size = size;
	m_hostVisible = isHostVisible(usage);

	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = acm::vulkan::toVk(usage);
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if (vkCreateBuffer(owner.vkDevice(), &bufferInfo, nullptr, &m_buffer) != VK_SUCCESS)
		return false;

	const VkMemoryPropertyFlags properties = m_hostVisible
												 ? (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
												 : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	VkMemoryRequirements requirements;
	vkGetBufferMemoryRequirements(owner.vkDevice(), m_buffer, &requirements);
	m_allocation = owner.allocator().allocate(requirements, properties);
	if (!m_allocation.valid())
		return false;
	vkBindBufferMemory(owner.vkDevice(), m_buffer, m_allocation.memory, m_allocation.offset);
	return true;
}

bool acm::vulkan::Buffer::isHostVisible(acm::BufferUsage usage)
{
	return usage == acm::BufferUsage::Uniform || usage == acm::BufferUsage::TransferDst || usage == acm::BufferUsage::Staging || usage == acm::BufferUsage::Storage;
}

size_t acm::vulkan::Buffer::size(const acm::Handle& handle) const
{
	return accessible(handle) ? m_size : 0;
}

void* acm::vulkan::Buffer::map(const acm::Handle& handle)
{
	return accessible(handle) && m_hostVisible ? m_allocation.mapped : nullptr;
}

VkBuffer acm::vulkan::Buffer::vkBuffer(const acm::Handle& handle) const
{
	return accessible(handle) ? m_buffer : VK_NULL_HANDLE;
}

acm::Error acm::vulkan::Buffer::write(const acm::Handle& handle, const void* data, size_t size)
{
	if (!accessible(handle))
		return acm::Error("Buffer::write: invalid buffer");
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
	if (acm::Error error = staging.native()->write(staging.handle(), data, bytes))
		return error;
	return owner().copyBuffer(staging.native()->vkBuffer(staging.handle()), m_buffer, VkDeviceSize(bytes));
}

void acm::vulkan::Buffer::retire(acm::vulkan::Device& owner)
{
	const VkBuffer buffer = std::exchange(m_buffer, VK_NULL_HANDLE);
	const acm::vulkan::Allocation allocation = std::exchange(m_allocation, {});
	m_size = 0;
	m_hostVisible = true;
	const VkDevice device = owner.vkDevice();
	if (buffer)
	{
		const VkBuffer retiredBuffer = buffer;
		owner.enqueueDestroy([device, retiredBuffer]
							 { vkDestroyBuffer(device, retiredBuffer, nullptr); });
	}
	if (allocation.valid())
	{
		acm::vulkan::MemoryAllocator* allocator = &owner.allocator();
		const acm::vulkan::Allocation retiredAllocation = allocation;
		owner.enqueueDestroy([allocator, retiredAllocation]
							 { allocator->free(retiredAllocation); });
	}
}
